import copy
import json
import os
import time
from datetime import datetime
from pathlib import Path

import torch


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

    det = yolo_obj.model           # inner nn.Module (DetectionModel)
    base_nc = getattr(det, "nc", None) or det.model[-1].nc
 
    if nc == base_nc:
        return det

    print(f"[INFO] Rebuilding Detect head for nc={base_nc} → nc={nc}")

    yaml_cfg = copy.deepcopy(det.yaml)
    yaml_cfg["nc"] = nc

    new_det = DetectionModel(yaml_cfg, ch=3, nc=nc)
    new_det.eval()

    orig_layers = list(det.model)
    new_layers  = list(new_det.model)

    with torch.no_grad():
        for orig_layer, new_layer in zip(orig_layers[:-1], new_layers[:-1]):
            new_layer.load_state_dict(orig_layer.state_dict(), strict=True)

    # Preserve stride (computed during build, needed for inference)
    detect_orig = orig_layers[-1]
    detect_new  = new_layers[-1]
    if hasattr(detect_orig, "stride") and detect_orig.stride is not None:
        detect_new.stride = detect_orig.stride.clone()

    print(f"[INFO] Backbone + neck transferred. "
          f"Total params: {sum(p.numel() for p in new_det.parameters()):,}")
    return new_det


class YOLOTrainer:

    def __init__(self, config_path: str):
        self.config_path = config_path
        self.config = None
        self.model = None        # YOLO wrapper (used for .train())
        self._det_model = None   # inner DetectionModel (used for inject/extract)

    def load_config(self):
        with open(self.config_path, "r") as f:
            self.config = json.load(f)

    # ── Weight helpers ────────────────────────────────────────────────────────

    def _extract_weights(self) -> list:
        """
        Extract all trainable tensors: parameters + BatchNorm running stats
        (running_mean, running_var). BN buffers are NOT parameters() but they
        are critical for correct inference — omitting them causes exploding
        activations in the aggregated model.
        """
        weights = []
        for p in self._det_model.parameters():
            weights.extend(p.detach().cpu().float().view(-1).tolist())
        # Append BN running statistics as extra floats at the end
        for name, buf in self._det_model.named_buffers():
            if "running_mean" in name or "running_var" in name:
                weights.extend(buf.detach().cpu().float().view(-1).tolist())
        return weights

    def _inject_weights(self, weights: list):
        """
        Inject weights in the same order as _extract_weights:
        first parameters(), then BN running_mean/running_var buffers.
        """
        flat = torch.tensor(weights, dtype=torch.float32)
        ptr  = 0

        with torch.no_grad():
            for param in self._det_model.parameters():
                numel = param.numel()
                if ptr + numel > len(flat):
                    raise ValueError(
                        f"[YOLOTrainer] Insufficient weights at ptr={ptr}, "
                        f"numel={numel}, total={len(flat)}. "
                        f"Server and client must use the same nc."
                    )
                param.data.copy_(flat[ptr:ptr + numel].view(param.shape))
                ptr += numel

            # Restore BN running statistics
            for name, buf in self._det_model.named_buffers():
                if "running_mean" in name or "running_var" in name:
                    numel = buf.numel()
                    if ptr + numel <= len(flat):
                        buf.data.copy_(flat[ptr:ptr + numel].view(buf.shape))
                        ptr += numel
                    # If not enough floats (old checkpoint without BN stats),
                    # leave the buffer as-is — inference will still work, just
                    # with default BN stats until the next round.

        # Accept both old format (params only) and new format (params + BN)
        if ptr != len(flat):
            print(
                f"[YOLOTrainer] Note: consumed={ptr}, received={len(flat)} floats. "
                f"Difference={len(flat)-ptr} — likely BN stats from a newer checkpoint."
            )

    # ── Build ─────────────────────────────────────────────────────────────────

    def build_model(self):
        from FedSolPython.scripts.unpack_dataset import unpack
        if unpack(self.config_path):
            self.load_config()

        from ultralytics import YOLO

        pt_path = self.config.get("model", {}).get("pt_path")
        if not pt_path:
            raise ValueError(
                "[YOLOTrainer] 'model.pt_path' is required in the config."
            )

        print(f"[INFO] Loading YOLO model from: {pt_path}")

        raw = _torch_load(pt_path)
        if isinstance(raw, YOLO):
            self.model = raw
        else:
            self.model = YOLO(pt_path)

        # Resolve target nc from the dataset config so the Detect head
        # matches the one the server built (and thus the weight count matches).
        nc = self.config.get("dataset", {}).get("nc", None)
        if nc is None:
            nc = self.model.model.nc
            print(f"[WARN] 'dataset.nc' not set — using base model nc={nc}.")

        # Rebuild the Detect head for the target nc if necessary.
        # This makes self._det_model the authoritative parameter store used
        # for both injection and extraction throughout the round.
        self._det_model = _rebuild_yolo_for_nc(self.model, nc)

        # Point the YOLO wrapper at the (possibly rebuilt) inner model so
        # self.model.train() trains the correct architecture.
        self.model.model = self._det_model

        # Inject federated weights from the server
        weights = self.config.get("model", {}).get("weights", [])
        if len(weights) > 1:
            print(f"[INFO] Applying {len(weights)} federated weights...")
            self._inject_weights(weights)
            print("[INFO] Federated weights applied successfully.")
        else:
            print("[INFO] No previous federated weights — using base model.")

    # ── Dataset YAML ─────────────────────────────────────────────────────────

    def _prepare_dataset_yaml(self) -> str:
        dataset_cfg = self.config["dataset"]

        train_path = str(Path(dataset_cfg["train_path"]).resolve())
        val_path   = dataset_cfg.get("val_path", train_path)
        val_path   = str(Path(val_path).resolve())

        nc    = dataset_cfg.get("nc", 1)
        names = dataset_cfg.get("names", [f"class_{i}" for i in range(nc)])

        names_yaml = "\n".join(f"  - {n}" for n in names)
        yaml_content = (
            f"train: {train_path}\n"
            f"val:   {val_path}\n"
            f"nc: {nc}\n"
            f"names:\n{names_yaml}\n"
        )

        config_dir = Path(self.config_path).parent
        yaml_path  = config_dir / "dataset_yolo.yaml"
        yaml_path.write_text(yaml_content)

        print(f"[INFO] Dataset YAML generated: {yaml_path}")
        return str(yaml_path)

    # ── Train ─────────────────────────────────────────────────────────────────

    def train(self):
        training_cfg = self.config["training"]
        epochs = training_cfg.get("epochs", 3)
        batch  = training_cfg.get("batch_size", 8)
        lr     = training_cfg.get("lr", 0.01)
        imgsz  = training_cfg.get("imgsz", 640)
        device = "cuda" if torch.cuda.is_available() else "cpu"

        yaml_path = self._prepare_dataset_yaml()

        now      = datetime.now().strftime("%Y%m%d_%H%M%S")
        project  = str(Path("output_client/yolo_runs").resolve())
        run_name = f"fed_{now}"

        print(f"\n[INFO] YOLO training — epochs={epochs}, batch={batch}, "
              f"lr={lr}, imgsz={imgsz}, device={device}")

        t_start = time.time()

        results = self.model.train(
            data=yaml_path,
            epochs=epochs,
            batch=batch,
            lr0=lr,
            imgsz=imgsz,
            device=device,
            project=project,
            name=run_name,
            verbose=True,
            amp=True,          # ← mixed precision, ~2x más rápido en GPU
            val=False,
            save=False,
            save_period=-1,
            exist_ok=True,
            workers=4,
            plots=False,
            cache=False,        # ← cachea imágenes en RAM, elimina I/O
            hsv_h=0.015, hsv_s=0.7, hsv_v=0.4,
            degrees=0.0, translate=0.1, scale=0.5,
            shear=0.0, perspective=0.0,
            flipud=0.0, fliplr=0.5,
            mosaic=0.0,        # ← mosaic es caro, deshabilitalo
            mixup=0.0,
            seed=42,
        )

        t_total = time.time() - t_start

        # Reload best checkpoint and sync _det_model so extraction is consistent
        best_pt = Path(project) / run_name / "weights" / "best.pt"
        if best_pt.exists():
            from ultralytics import YOLO as _YOLO
            reloaded = _torch_load(str(best_pt))
            if not isinstance(reloaded, _YOLO):
                reloaded = _YOLO(str(best_pt))
            self.model     = reloaded
            self._det_model = reloaded.model
            print(f"[INFO] Best model reloaded from: {best_pt}")
        else:
            print("[WARNING] best.pt not found — using the latest model state.")

        stats = self._build_stats(results, t_total, epochs, batch, lr, device)
        self._save_stats(stats)
        self._update_weights_in_json()

        print("\n[TRAINING DONE]")
        return stats

    # ── Stats / persistence ───────────────────────────────────────────────────

    def _build_stats(self, results, t_total, epochs, batch, lr, device) -> dict:
        stats = {
            "config_path":                  self.config_path,
            "device":                       device,
            "epochs":                       epochs,
            "batch_size":                   batch,
            "learning_rate":                lr,
            "total_training_time_seconds":  t_total,
            "timestamp":                    datetime.now().isoformat(),
            "weights_count":                len(self._extract_weights()),
        }
        try:
            stats["metrics"] = {
                k: float(v)
                for k, v in results.results_dict.items()
                if isinstance(v, (int, float))
            }
        except Exception:
            stats["metrics"] = {}
        return stats

    def _save_stats(self, stats: dict):
        now    = datetime.now()
        folder = f"output_client/stats/{now.strftime('%Y-%m-%d')}"
        os.makedirs(folder, exist_ok=True)
        path   = f"{folder}/run_{now.strftime('%H-%M-%S')}.json"
        with open(path, "w") as f:
            json.dump(stats, f, indent=4)
        print(f"[INFO] Stats saved: {path}")

    def _update_weights_in_json(self):
        weights = self._extract_weights()
        self.config["model"]["weights"] = weights
        with open(self.config_path, "w") as f:
            json.dump(self.config, f, indent=4)
        print(f"[INFO] Weights written to JSON: {len(weights):,} floats")

    def run(self):
        print("[INFO] Starting federated YOLOTrainer pipeline...")
        self.load_config()
        self.build_model()
        self.train()
        print("[INFO] YOLOTrainer pipeline finished.")