import json
import numpy as np
import sys
from pathlib import Path

def count_yolo_samples(dataset_cfg: dict) -> int:
    """
    Cuenta las imágenes en el directorio de entrenamiento YOLO.
    Busca en train_path/images/ o directamente en train_path/ si no hay subdirectorio images/.
    """
    train_path = Path(dataset_cfg["train_path"])
 
    images_dir = train_path / "images"
    if not images_dir.exists():
        images_dir = train_path
 
    extensions = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff", ".webp"}
    count = sum(1 for f in images_dir.rglob("*") if f.suffix.lower() in extensions)
    return count
 
 
def count_npz_samples(dataset_cfg: dict) -> int:
    import numpy as np
    path = dataset_cfg["train_path"]
    data = np.load(path, allow_pickle=True)
    if "X" not in data:
        print("Key 'X' not found in NPZ", file=sys.stderr)
        sys.exit(1)
    return int(data["X"].shape[0])

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 get_total_samples.py <config.json>", file=sys.stderr)
        sys.exit(1)
 
    config_path = sys.argv[1]
 
    with open(config_path, "r") as f:
        config = json.load(f)
 
    try:
        dataset_cfg = config["dataset"]
    except KeyError:
        print("dataset not found in config", file=sys.stderr)
        sys.exit(1)
 
    dataset_type = dataset_cfg.get("type", "npz")
 
    if dataset_type == "yolo":
        count = count_yolo_samples(dataset_cfg)
    elif dataset_type == "npz":
        count = count_npz_samples(dataset_cfg)
    else:
        print(f"Unsupported dataset type: {dataset_type}", file=sys.stderr)
        sys.exit(1)
 
    print(count)
 
 
if __name__ == "__main__":
    main()