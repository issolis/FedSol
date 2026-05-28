import copy
import sys
import json
import torch
from FedSolPython.training.ModelBuilder import ModelBuilder
from FedSolPython.utils.Logger import Logger


def _load_model(path, map_location="cpu"):
    """
    Loads a .pt file. Tries YOLO() first (handles Ultralytics dict-format files
    saved with model.save() or YOLO.save()), falls back to torch.load for
    generic nn.Module objects.
    """
    try:
        from ultralytics import YOLO
        m = YOLO(path)
        # Verify it actually loaded a model, not just metadata
        if hasattr(m, "model") and hasattr(m.model, "parameters"):
            return m
    except Exception:
        pass
    try:
        return torch.load(path, map_location=map_location, weights_only=False)
    except TypeError:
        return torch.load(path, map_location=map_location)


def _torch_load(path, map_location="cpu"):
    return _load_model(path, map_location)


def _rebuild_yolo_for_nc(yolo_obj, nc: int):
    from ultralytics.nn.tasks import DetectionModel

    det = yolo_obj.model
    base_nc = getattr(det, "nc", None) or det.model[-1].nc

    if nc == base_nc:
        return det

    Logger.info(f"Rebuilding Detect head: nc={base_nc} → nc={nc}")

    yaml_cfg = copy.deepcopy(det.yaml)
    yaml_cfg["nc"] = nc

    new_det = DetectionModel(yaml_cfg, ch=3, nc=nc)
    new_det.eval()

    orig_layers = list(det.model)
    new_layers  = list(new_det.model)

    with torch.no_grad():
        for orig_layer, new_layer in zip(orig_layers[:-1], new_layers[:-1]):
            new_layer.load_state_dict(orig_layer.state_dict(), strict=True)

    detect_orig = orig_layers[-1]
    detect_new  = new_layers[-1]
    if hasattr(detect_orig, "stride") and detect_orig.stride is not None:
        detect_new.stride = detect_orig.stride.clone()

    Logger.info(f"Backbone + neck transferred. "
                f"Total params: {sum(p.numel() for p in new_det.parameters()):,}")
    return new_det


def _inject_weights_generic(det_model, weights: list):
    """
    Inject federated weights into det_model in the same order as
    YOLOTrainer._extract_weights(): parameters() first, then BN
    running_mean/running_var buffers.
    """
    flat = torch.tensor(weights, dtype=torch.float32)
    ptr  = 0

    with torch.no_grad():
        for param in det_model.parameters():
            numel = param.numel()
            if ptr + numel > len(flat):
                raise ValueError(
                    f"[build_and_save] Pesos insuficientes: ptr={ptr}, "
                    f"numel={numel}, total={len(flat)}"
                )
            param.data.copy_(flat[ptr:ptr + numel].view(param.shape))
            ptr += numel

        # Restore BN running statistics (running_mean, running_var)
        # These are buffers, not parameters — critical for correct inference.
        for name, buf in det_model.named_buffers():
            if "running_mean" in name or "running_var" in name:
                numel = buf.numel()
                if ptr + numel <= len(flat):
                    buf.data.copy_(flat[ptr:ptr + numel].view(buf.shape))
                    ptr += numel
                # If no BN stats in payload (old checkpoint), leave defaults.

    if ptr != len(flat):
        Logger.warning(
            f"[build_and_save] consumed={ptr}, received={len(flat)} floats. "
            f"Excess={len(flat)-ptr}."
        )


def main():
    if len(sys.argv) < 2:
        Logger.error("Usage: python3 build_and_save.py <config_path>")
        return 1

    config_path = sys.argv[1]

    Logger.configure("output_server/logs/server.log")

    try:
        Logger.info(f"Starting build from config: {config_path}")

        with open(config_path, "r") as f:
            config = json.load(f)

        pt_path = config.get("model", {}).get("pt_path")

        if pt_path:
            # -- pt_path mode --------------------------------------------------
            Logger.info(f"pt_path mode: loading model from '{pt_path}'")

            yolo_obj = _torch_load(pt_path)

            weights = config.get("model", {}).get("weights", [])

            if len(weights) <= 1:
                Logger.warning("No valid weights in config — skipping weight injection")
                # Use the base model as-is for archiving
                torch_model = yolo_obj
            else:
                is_yolo_wrapper = hasattr(yolo_obj, "model") and hasattr(yolo_obj.model, "yaml")

                if is_yolo_wrapper:
                    # Resolve the target nc from dataset config so the Detect
                    # head matches what the server and clients agreed on.
                    nc = config.get("dataset", {}).get("nc", None)
                    if nc is None:
                        nc = yolo_obj.model.nc
                        Logger.warning(
                            f"'dataset.nc' not set — using base model nc={nc}"
                        )

                    det_model = _rebuild_yolo_for_nc(yolo_obj, nc)

                    # Point the YOLO wrapper at the rebuilt inner model so
                    # torch.save(yolo_obj, ...) persists the correct architecture.
                    yolo_obj.model = det_model

                    # Sync class names from dataset config into the YOLO wrapper
                    names = config.get("dataset", {}).get("names", [])
                    if names:
                        yolo_obj.model.names = {i: n for i, n in enumerate(names)}

                    _inject_weights_generic(det_model, weights)
                    Logger.info(
                        f"Federated weights applied to YOLO (nc={nc}): "
                        f"{len(weights):,} floats"
                    )

                elif isinstance(yolo_obj, torch.nn.Sequential):
                    builder = ModelBuilder(config_path)
                    builder.load()
                    builder.load_weights(yolo_obj)
                    Logger.info("Federated weights applied via ModelBuilder")

                else:
                    _inject_weights_generic(yolo_obj, weights)
                    Logger.info(
                        f"Federated weights applied via parameters(): "
                        f"{len(weights):,} floats"
                    )

                torch_model = yolo_obj

            # Archive copy with timestamp
            import os as _os
            from datetime import datetime as _dt
            _os.makedirs("output_server/models", exist_ok=True)
            _timestamp = _dt.now().strftime("%Y%m%d_%H%M%S")
            saved_path = f"output_server/models/model_{_timestamp}.pt"

            # Overwrite input_server so next round starts from the updated model
            if is_yolo_wrapper:
                from ultralytics import YOLO as _YOLO
                fresh = _YOLO(pt_path)  # load original architecture
                # Rebuild head for nc if needed
                fresh_nc = getattr(fresh.model, "nc", None) or fresh.model.model[-1].nc
                if fresh_nc != nc:
                    import copy as _copy
                    from ultralytics.nn.tasks import DetectionModel as _DM
                    _yaml = _copy.deepcopy(fresh.model.yaml)
                    _yaml["nc"] = nc
                    _new_det = _DM(_yaml, ch=3, nc=nc)
                    _orig = list(fresh.model.model)
                    _new = list(_new_det.model)
                    with torch.no_grad():
                        for _ol, _nl in zip(_orig[:-1], _new[:-1]):
                            _nl.load_state_dict(_ol.state_dict(), strict=True)
                    if hasattr(_orig[-1], "stride") and _orig[-1].stride is not None:
                        _new[-1].stride = _orig[-1].stride.clone()
                    fresh.model = _new_det
                # Copy federated weights into fresh model via full state_dict
                # (includes BN running_mean/running_var buffers, not just parameters)
                fresh.model.load_state_dict(det_model.state_dict(), strict=True)
                # Sync names
                if names:
                    fresh.model.names = {i: n for i, n in enumerate(names)}
                torch.save(fresh, saved_path)
                torch.save(fresh, pt_path)
            else:
                torch.save(torch_model, saved_path)
                torch.save(torch_model, pt_path)

            Logger.info(f"Archived model saved to: {saved_path}")
            Logger.info(f"Updated .pt written back to: {pt_path}")

        else:
            # -- Normal mode (architecture built from JSON) --------------------
            builder = ModelBuilder(config_path)
            model = builder.build()
            saved_path = builder.save_model(model)
            Logger.info(f"Build/Save completed successfully: {saved_path}")

        return 0

    except Exception as e:
        Logger.error(f"Build/Save failed: {e}")
        return 1


if __name__ == "__main__":
    exit_code = main()
    sys.exit(exit_code)