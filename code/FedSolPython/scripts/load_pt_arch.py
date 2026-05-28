import sys
import json
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

import torch.nn as nn


# ── Activation type codes (match FedSol ActivationType enum) ──────────────────
ACTIVATION_MAP = {
    nn.ReLU:    2,
    nn.Sigmoid: 3,
    nn.Tanh:    4,
    nn.Softmax: 5,
}

LAYER_CONV2D     = 2
LAYER_MAXPOOL2D  = 3
LAYER_FLATTEN    = 4
LAYER_DENSE      = 5
LAYER_BATCHNORM  = 6
LAYER_DROPOUT    = 7
LAYER_ACTIVATION = 8


def _infer_layers(model: nn.Module) -> list:
    if isinstance(model, nn.Sequential):
        children = list(model.children())
    else:
        all_mods = list(model.modules())
        children = [m for m in all_mods[1:] if len(list(m.children())) == 0]

    layers = []

    for mod in children:

        if isinstance(mod, nn.Conv2d):
            k = mod.kernel_size[0] if isinstance(mod.kernel_size, tuple) else mod.kernel_size
            s = mod.stride[0]      if isinstance(mod.stride, tuple)      else mod.stride
            p = mod.padding[0]     if isinstance(mod.padding, tuple)     else mod.padding
            layers.append({"type": LAYER_CONV2D, "input_dim": [0, 0, mod.in_channels],
                "output_dim": [0, 0, mod.out_channels], "kernel_size": k, "stride": s,
                "padding": p, "in_features": 0, "out_features": 0, "activation": 0})

        elif isinstance(mod, nn.MaxPool2d):
            k = mod.kernel_size if isinstance(mod.kernel_size, int) else mod.kernel_size[0]
            s = mod.stride      if isinstance(mod.stride, int)      else mod.stride[0]
            layers.append({"type": LAYER_MAXPOOL2D, "input_dim": [0, 0, 0],
                "output_dim": [0, 0, 0], "kernel_size": k, "stride": s,
                "padding": 0, "in_features": 0, "out_features": 0, "activation": 0})

        elif isinstance(mod, nn.Flatten):
            layers.append({"type": LAYER_FLATTEN, "input_dim": [0, 0, 0],
                "output_dim": [0, 0, 0], "kernel_size": 0, "stride": 0,
                "padding": 0, "in_features": 0, "out_features": 0, "activation": 0})

        elif isinstance(mod, nn.Linear):
            layers.append({"type": LAYER_DENSE, "input_dim": [0, 0, 0],
                "output_dim": [0, 0, 0], "kernel_size": 0, "stride": 0, "padding": 0,
                "in_features": mod.in_features, "out_features": mod.out_features, "activation": 0})

        elif isinstance(mod, (nn.BatchNorm2d, nn.BatchNorm1d)):
            layers.append({"type": LAYER_BATCHNORM, "input_dim": [0, 0, mod.num_features],
                "output_dim": [0, 0, mod.num_features], "kernel_size": 0, "stride": 0,
                "padding": 0, "in_features": 0, "out_features": 0, "activation": 0})

        elif isinstance(mod, (nn.Dropout, nn.Dropout2d)):
            layers.append({"type": LAYER_DROPOUT, "input_dim": [0, 0, 0],
                "output_dim": [0, 0, 0], "kernel_size": 0, "stride": 0,
                "padding": 0, "in_features": 0, "out_features": 0, "activation": 0})

        elif type(mod) in ACTIVATION_MAP:
            layers.append({"type": LAYER_ACTIVATION, "input_dim": [0, 0, 0],
                "output_dim": [0, 0, 0], "kernel_size": 0, "stride": 0, "padding": 0,
                "in_features": 0, "out_features": 0, "activation": ACTIVATION_MAP[type(mod)]})

        elif isinstance(mod, nn.Identity):
            pass

        else:
            print(f"[WARN] Unmapped module ignored: {type(mod).__name__}")

    return layers


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 -m FedSolPython.scripts.load_pt_arch <config_path>")
        sys.exit(1)

    config_path = sys.argv[1]

    with open(config_path, "r") as f:
        config = json.load(f)

    pt_path = config.get("model", {}).get("pt_path")
    if not pt_path:
        print("[ERROR] 'model.pt_path' not found in config.")
        sys.exit(1)

    print(f"[INFO] Loading model from: {pt_path}")

    try:
        model = _torch_load(pt_path)
    except Exception as e:
        print(f"[ERROR] Failed to load .pt: {e}")
        sys.exit(1)

    is_yolo = hasattr(model, "model") and hasattr(model.model, "yaml")
    if is_yolo:
        model.model.eval()
    else:
        model.eval()
    print(f"[INFO] Model loaded: {type(model).__name__}")

    print("[INFO] Inferring architecture from model...")
    # For YOLO, infer layers from the inner DetectionModel
    layers = _infer_layers(model.model if is_yolo else model)

    if not layers:
        print("[ERROR] Could not infer any layers from the model.")
        sys.exit(1)

    config["model"]["architecture"] = {"layers": layers}
    print(f"[INFO] Architecture inferred: {len(layers)} layers")

    # Weights are intentionally NOT written — the server sends them at handshake.

    with open(config_path, "w") as f:
        json.dump(config, f, indent=4)

    print(f"[INFO] Config updated: {config_path}")
    print("PT_ARCH_OK")


if __name__ == "__main__":
    main()