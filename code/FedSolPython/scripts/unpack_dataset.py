import sys
import json
import tarfile
import shutil
from pathlib import Path


def unpack(config_path: str) -> bool:
    with open(config_path, "r") as f:
        config = json.load(f)

    dataset_cfg = config.get("dataset", {})

    receive_path = dataset_cfg.get("receive_path")
    if not receive_path:
        return False

    tar_path = Path(receive_path)
    if not tar_path.exists():
        return False

    if ".tar.gz" not in tar_path.name and tar_path.suffix not in (".gz", ".tar"):
        return False

    config_dir = Path(config_path).parent
    extract_to = config_dir / "dataset_local"

    if extract_to.exists():
        shutil.rmtree(str(extract_to))

    extract_to.mkdir(parents=True, exist_ok=True)

    print(f"[unpack_dataset] Unpacking {tar_path} → {extract_to}")

    with tarfile.open(str(tar_path), "r:gz") as tar:
        try:
            tar.extractall(str(extract_to), filter="data")
        except TypeError:
            tar.extractall(str(extract_to))

    dataset_inner = extract_to / "dataset"
    if not dataset_inner.exists():
        dataset_inner = extract_to 

    _update_paths(config, config_path, dataset_inner)

    print(f"[unpack_dataset] Dataset at: {dataset_inner}")
    return True


def _update_paths(config: dict, config_path: str, dataset_root: Path):
    train_dir = dataset_root / "train"
    val_dir   = dataset_root / "val"

    config["dataset"]["train_path"] = str(train_dir.resolve())
    config["dataset"]["val_path"] = str(val_dir.resolve()) if val_dir.exists() \
                                    else str(train_dir.resolve())

    with open(config_path, "w") as f:
        json.dump(config, f, indent=4)

    print(f"[unpack_dataset] train_path → {config['dataset']['train_path']}")
    print(f"[unpack_dataset] val_path   → {config['dataset']['val_path']}")


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 -m FedSolPython.scripts.unpack_dataset <config_path>")
        sys.exit(1)

    unpacked = unpack(sys.argv[1])
    if not unpacked:
        print("[unpack_dataset] Nothing to unpack.")


if __name__ == "__main__":
    main()