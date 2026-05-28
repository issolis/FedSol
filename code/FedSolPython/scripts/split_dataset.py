import sys
import os
import math
import tarfile
import shutil
import numpy as np
from pathlib import Path

from FedSolPython.utils.Logger import Logger

def split_npz(npz_path: str, n_clients: int, output_dir: str = "input_server") -> None:
    data = np.load(npz_path)
    X = data["X"]
    y = data["y"]

    total_samples = X.shape[0]
    Logger.info(f"[split_dataset] NPZ — {total_samples} samples → {n_clients} clients")

    X_splits = np.array_split(X, n_clients)
    y_splits = np.array_split(y, n_clients)

    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    for i, (X_part, y_part) in enumerate(zip(X_splits, y_splits)):
        out_file = output_path / f"dataset_train_{i}.npz"
        np.savez(str(out_file), X=X_part, y=y_part)
        Logger.info(f"[split_dataset] Client {i}: {X_part.shape[0]} samples → {out_file}")

    Logger.info(f"[split_dataset] Done. {n_clients} .npz files saved to '{output_dir}/'")


YOLO_IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff", ".webp"}

def split_yolo(dataset_root: str, n_clients: int, output_dir: str = "input_server") -> None:
    root = Path(dataset_root)

    train_images_dir = root / "train" / "images"
    train_labels_dir = root / "train" / "labels"
    val_dir          = root / "val"

    if not train_images_dir.exists():
        Logger.error(f"[split_dataset] Could not find {train_images_dir}")
        print(f"[ERROR] Could not find {train_images_dir}", file=sys.stderr)
        sys.exit(1)

    all_images = sorted([
        f for f in train_images_dir.iterdir()
        if f.suffix.lower() in YOLO_IMAGE_EXTENSIONS
    ])

    total = len(all_images)
    if total == 0:
        Logger.error(f"[split_dataset] No images found in {train_images_dir}")
        print(f"[ERROR] No images found in {train_images_dir}", file=sys.stderr)
        sys.exit(1)

    per_client = math.ceil(total / n_clients)
    Logger.info(f"[split_dataset] YOLO — {total} images → {n_clients} clients (~{per_client} each)")

    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    for i in range(n_clients):
        chunk = all_images[i * per_client : (i + 1) * per_client]
        if not chunk:
            Logger.info(f"[split_dataset] Client {i}: no images assigned, dataset is too small")
            continue

        tmp_dir = output_path / f"_tmp_client_{i}"
        client_train_img = tmp_dir / "train" / "images"
        client_train_lbl = tmp_dir / "train" / "labels"
        client_train_img.mkdir(parents=True, exist_ok=True)
        client_train_lbl.mkdir(parents=True, exist_ok=True)

        try:
            for img_path in chunk:
                shutil.copy2(img_path, client_train_img / img_path.name)
                lbl_path = train_labels_dir / (img_path.stem + ".txt")
                if lbl_path.exists():
                    shutil.copy2(lbl_path, client_train_lbl / lbl_path.name)

            if val_dir.exists():
                shutil.copytree(str(val_dir), str(tmp_dir / "val"), dirs_exist_ok=True)

            tar_path = output_path / f"dataset_train_{i}.tar.gz"
            with tarfile.open(str(tar_path), "w:gz") as tar:
                tar.add(str(tmp_dir), arcname="dataset")

        finally:
            if tmp_dir.exists():
                shutil.rmtree(str(tmp_dir))

        tar_mb = tar_path.stat().st_size / (1024 * 1024)
        Logger.info(
            f"[split_dataset] Client {i}: {len(chunk)} images "
            f"→ {tar_path.name} ({tar_mb:.1f} MB)"
        )

    Logger.info(f"[split_dataset] Done. {n_clients} .tar.gz files saved to '{output_dir}/'")

def main():
    if len(sys.argv) < 3:
        Logger.error(
            "[split_dataset] Usage: python3 -m FedSolPython.scripts.split_dataset "
            "<dataset_path> <n_clients>"
        )
        sys.exit(1)

    Logger.configure("output_server/logs/server.log")

    dataset_path = sys.argv[1]
    n_clients    = int(sys.argv[2])
    path         = Path(dataset_path)

    if path.is_dir():
        split_yolo(dataset_path, n_clients)
    elif path.suffix == ".npz":
        split_npz(dataset_path, n_clients)
    else:
        msg = (
            f"[split_dataset] Unsupported format: '{path.suffix}'. "
            f"Use a .npz file or a YOLO directory."
        )
        Logger.error(msg)
        print(msg, file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()