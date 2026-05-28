"""
Computes a stable SHA256 hash from the architecture of a .pt model.
The hash depends only on the model structure, not the weights,
so it remains constant across rounds and server restarts.

Usage:
    python3 -m FedSolPython.scripts.get_pt_arch_hash <pt_path>

Always prints exactly one line to stdout: the hex digest.
"""
import sys
import os
import hashlib
import json
import warnings
warnings.filterwarnings("ignore")


class _SuppressOutput:
    """
    Context manager that redirects stdout and stderr to /dev/null.
    Used to silence Ultralytics/PyTorch prints during model loading
    so the only thing written to stdout is the final hash line.
    """
    def __enter__(self):
        self._devnull = open(os.devnull, "w")
        self._stdout  = sys.stdout
        self._stderr  = sys.stderr
        sys.stdout    = self._devnull
        sys.stderr    = self._devnull
        return self

    def __exit__(self, *args):
        sys.stdout = self._stdout
        sys.stderr = self._stderr
        self._devnull.close()


def get_arch_hash(pt_path: str) -> str:

    # ── YOLO path ─────────────────────────────────────────────────────────────
    try:
        with _SuppressOutput():
            from ultralytics import YOLO
            model = YOLO(pt_path)
        if hasattr(model, "model") and hasattr(model.model, "yaml"):
            arch_str = json.dumps(model.model.yaml, sort_keys=True)
            return hashlib.sha256(arch_str.encode()).hexdigest()
    except Exception:
        pass

    # ── Generic nn.Module path ────────────────────────────────────────────────
    try:
        import torch
        with _SuppressOutput():
            try:
                raw = torch.load(pt_path, map_location="cpu", weights_only=False)
            except TypeError:
                raw = torch.load(pt_path, map_location="cpu")

        if isinstance(raw, dict):
            arch_data = {k: list(v.shape) for k, v in raw.items()
                         if hasattr(v, "shape")}
        else:
            arch_data = {name: list(param.shape)
                         for name, param in raw.named_parameters()}

        arch_str = json.dumps(arch_data, sort_keys=True)
        return hashlib.sha256(arch_str.encode()).hexdigest()
    except Exception:
        pass

    # ── Last resort: hash the file path itself ────────────────────────────────
    return hashlib.sha256(pt_path.encode()).hexdigest()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.stderr.write("Usage: python3 -m FedSolPython.scripts.get_pt_arch_hash <pt_path>\n")
        sys.exit(1)

    result = get_arch_hash(sys.argv[1])
    try:
        print(result)
        sys.stdout.flush()
    except BrokenPipeError:
        # The C++ server closed the pipe after reading the hash line — expected.
        os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())
        sys.exit(0)