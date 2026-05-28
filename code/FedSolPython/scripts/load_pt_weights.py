import sys
import json
import copy
import torch
import torch.nn as nn

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


# ── Activation type codes (match FedSol ActivationType enum) ──────────────────
ACTIVATION_MAP = {
    nn.ReLU:    2,   # ACT_RELU
    nn.Sigmoid: 3,   # ACT_SIGMOID
    nn.Tanh:    4,   # ACT_TANH
    nn.Softmax: 5,   # ACT_SOFTMAX
}

# ── Layer type codes (match FedSol LayerType enum) ────────────────────────────
LAYER_INPUT      = 1
LAYER_CONV2D     = 2
LAYER_MAXPOOL2D  = 3
LAYER_FLATTEN    = 4
LAYER_DENSE      = 5
LAYER_BATCHNORM  = 6
LAYER_DROPOUT    = 7
LAYER_ACTIVATION = 8


def _infer_layers(model: nn.Module) -> list:
    """
    Walks the model's modules and builds the layer list in the FedSol DSL.

    For nn.Sequential, iterates children directly.
    For any other model, collects leaf modules (no children of their own)
    to avoid entering intermediate blocks (e.g. BasicBlock in ResNet).
    """
    if isinstance(model, nn.Sequential):
        children = list(model.children())
    else:
        all_mods = list(model.modules())
        # Skip the root module (index 0) and keep only leaf modules
        children = [m for m in all_mods[1:] if len(list(m.children())) == 0]

    layers = []

    for mod in children:

        # ── Conv2d ────────────────────────────────────────────────────────────
        if isinstance(mod, nn.Conv2d):
            k = mod.kernel_size[0] if isinstance(mod.kernel_size, tuple) else mod.kernel_size
            s = mod.stride[0]      if isinstance(mod.stride, tuple)      else mod.stride
            p = mod.padding[0]     if isinstance(mod.padding, tuple)     else mod.padding

            layers.append({
                "type":        LAYER_CONV2D,
                "input_dim":   [0, 0, mod.in_channels],
                "output_dim":  [0, 0, mod.out_channels],
                "kernel_size": k,
                "stride":      s,
                "padding":     p,
                "in_features":  0,
                "out_features": 0,
                "activation":   0,
            })

        # ── MaxPool2d ─────────────────────────────────────────────────────────
        elif isinstance(mod, nn.MaxPool2d):
            k = mod.kernel_size if isinstance(mod.kernel_size, int) else mod.kernel_size[0]
            s = mod.stride      if isinstance(mod.stride, int)      else mod.stride[0]

            layers.append({
                "type":        LAYER_MAXPOOL2D,
                "input_dim":   [0, 0, 0],
                "output_dim":  [0, 0, 0],
                "kernel_size": k,
                "stride":      s,
                "padding":     0,
                "in_features":  0,
                "out_features": 0,
                "activation":   0,
            })

        # ── Flatten ───────────────────────────────────────────────────────────
        elif isinstance(mod, nn.Flatten):
            layers.append({
                "type":        LAYER_FLATTEN,
                "input_dim":   [0, 0, 0],
                "output_dim":  [0, 0, 0],
                "kernel_size": 0,
                "stride":      0,
                "padding":     0,
                "in_features":  0,
                "out_features": 0,
                "activation":   0,
            })

        # ── Linear ────────────────────────────────────────────────────────────
        elif isinstance(mod, nn.Linear):
            layers.append({
                "type":        LAYER_DENSE,
                "input_dim":   [0, 0, 0],
                "output_dim":  [0, 0, 0],
                "kernel_size": 0,
                "stride":      0,
                "padding":     0,
                "in_features":  mod.in_features,
                "out_features": mod.out_features,
                "activation":   0,
            })

        # ── BatchNorm ─────────────────────────────────────────────────────────
        elif isinstance(mod, (nn.BatchNorm2d, nn.BatchNorm1d)):
            layers.append({
                "type":        LAYER_BATCHNORM,
                "input_dim":   [0, 0, mod.num_features],
                "output_dim":  [0, 0, mod.num_features],
                "kernel_size": 0,
                "stride":      0,
                "padding":     0,
                "in_features":  0,
                "out_features": 0,
                "activation":   0,
            })

        # ── Dropout ───────────────────────────────────────────────────────────
        elif isinstance(mod, (nn.Dropout, nn.Dropout2d)):
            layers.append({
                "type":        LAYER_DROPOUT,
                "input_dim":   [0, 0, 0],
                "output_dim":  [0, 0, 0],
                "kernel_size": 0,
                "stride":      0,
                "padding":     0,
                "in_features":  0,
                "out_features": 0,
                "activation":   0,
            })

        # ── Activations ───────────────────────────────────────────────────────
        elif type(mod) in ACTIVATION_MAP:
            layers.append({
                "type":        LAYER_ACTIVATION,
                "input_dim":   [0, 0, 0],
                "output_dim":  [0, 0, 0],
                "kernel_size": 0,
                "stride":      0,
                "padding":     0,
                "in_features":  0,
                "out_features": 0,
                "activation":   ACTIVATION_MAP[type(mod)],
            })

        elif isinstance(mod, nn.Identity):
            pass  # No parameters, safe to skip

        else:
            print(f"[WARN] Unmapped module ignored: {type(mod).__name__}")

    return layers


def _rebuild_yolo_for_nc(model, nc: int):
    from ultralytics.nn.tasks import DetectionModel

    base_nc = getattr(model.model, "nc", None) or model.model.model[-1].nc
    if nc == base_nc:
        print(f"[INFO] nc={nc} matches base model — no head rebuild needed.")
        return model.model

    print(f"[INFO] Rebuilding Detect head: nc={base_nc} → nc={nc}")

    yaml_cfg = copy.deepcopy(model.model.yaml)
    yaml_cfg["nc"] = nc

    new_det = DetectionModel(yaml_cfg, ch=3, nc=nc)
    new_det.eval()

    # ── Copy backbone + neck weights (all layers except the Detect head) ──────
    # Both models are built from the same yaml, so every layer except [-1]
    # (the Detect head) has identical structure and parameter shapes.
    orig_layers = list(model.model.model)
    new_layers  = list(new_det.model)

    with torch.no_grad():
        for orig_layer, new_layer in zip(orig_layers[:-1], new_layers[:-1]):
            orig_sd = orig_layer.state_dict()
            new_layer.load_state_dict(orig_sd, strict=True)

    print(f"[INFO] Backbone + neck weights transferred ({len(orig_layers) - 1} layers).")

    # Preserve stride info computed during the original build
    detect_orig = orig_layers[-1]
    detect_new  = new_layers[-1]
    if hasattr(detect_orig, "stride") and detect_orig.stride is not None:
        detect_new.stride = detect_orig.stride.clone()

    return new_det


def extract_weights_flat(det_model: nn.Module) -> list:
    """
    Extracts all trainable parameters AND BatchNorm running statistics
    of a DetectionModel (or any nn.Module) into a single flat float32 list.

    Order (must match YOLOTrainer._extract_weights and _inject_weights):
      1. parameters()         — conv weights, BN gamma/beta, biases
      2. named_buffers()      — BN running_mean, running_var (in iteration order)

    BN running statistics are NOT parameters() — they have no gradient and are
    updated during the forward pass. Omitting them causes the aggregated model
    to use default BN stats (mean=0, var=1) which produce exploding activations
    and conf=1.000 on every detection.
    """
    weights = []
    for p in det_model.parameters():
        weights.extend(p.detach().cpu().float().numpy().flatten().tolist())
    for name, buf in det_model.named_buffers():
        if "running_mean" in name or "running_var" in name:
            weights.extend(buf.detach().cpu().float().numpy().flatten().tolist())
    return weights


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 -m FedSolPython.scripts.load_pt_weights <config_path>")
        sys.exit(1)

    config_path = sys.argv[1]

    # ── Read config ───────────────────────────────────────────────────────────
    with open(config_path, "r") as f:
        config = json.load(f)

    pt_path = config.get("model", {}).get("pt_path")
    if not pt_path:
        print("[ERROR] 'model.pt_path' not found in config.")
        sys.exit(1)
        
    nc = config.get("dataset", {}).get("nc", None)
    if nc is None:
        print("[WARN] 'dataset.nc' not set in config — defaulting to base model nc.")

    print(f"[INFO] Loading model from: {pt_path}")

    # ── Load .pt ──────────────────────────────────────────────────────────────
    try:
        model = _torch_load(pt_path)
    except Exception as e:
        print(f"[ERROR] Failed to load .pt: {e}")
        sys.exit(1)

    if isinstance(model, dict):
        print("[ERROR] model.pt contains a state_dict, not a full model object.")
        print("[ERROR] Re-run init_yolo_model.py to regenerate a valid model.pt.")
        sys.exit(1)

    is_yolo_wrapper = hasattr(model, "model") and hasattr(model.model, "yaml")

    if is_yolo_wrapper:
        model.model.eval()
    else:
        model.eval()
    print(f"[INFO] Model loaded: {type(model).__name__}")

    if is_yolo_wrapper:
        base_nc = getattr(model.model, "nc", None) or model.model.model[-1].nc
        target_nc = nc if nc is not None else base_nc
        print(f"[INFO] Base model nc={base_nc}, target nc={target_nc}")

        # Rebuild with the target nc so param count matches the clients
        det_model = _rebuild_yolo_for_nc(model, target_nc)

        total_params = sum(p.numel() for p in det_model.parameters())
        print(f"[INFO] DetectionModel (nc={target_nc}) has {total_params:,} parameters.")

        # ── Infer architecture from the rebuilt model ─────────────────────────
        print("[INFO] Inferring architecture from model...")
        layers = _infer_layers(det_model)
        if not layers:
            print("[ERROR] Could not infer any layers from the model.")
            sys.exit(1)
        config["model"]["architecture"] = {"layers": layers}
        print(f"[INFO] Architecture inferred: {len(layers)} layers")

        # ── Extract weights ───────────────────────────────────────────────────
        weights = extract_weights_flat(det_model)

    else:
        print("[INFO] Inferring architecture from model...")
        layers = _infer_layers(model)
        if not layers:
            print("[ERROR] Could not infer any layers from the model.")
            sys.exit(1)
        config["model"]["architecture"] = {"layers": layers}
        print(f"[INFO] Architecture inferred: {len(layers)} layers")

        weights = extract_weights_flat(model)

    config["model"]["weights"] = weights
    print(f"[INFO] Weights extracted: {len(weights):,} floats")

    # ── Write updated config ──────────────────────────────────────────────────
    with open(config_path, "w") as f:
        json.dump(config, f, indent=4)

    print(f"[INFO] Config updated: {config_path}")
    print("PT_LOAD_OK")


if __name__ == "__main__":
    main()