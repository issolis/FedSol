import argparse
import os
import torch


def parse_args():
    p = argparse.ArgumentParser(description="Descarga modelo YOLO base para FedSol (si no existe)")
    p.add_argument("--model",  default="yolov8n.pt",          help="Modelo Ultralytics a descargar (yolov8n/s/m/l/x)")
    p.add_argument("--output", default="input_server/model.pt", help="Ruta de destino del .pt")
    return p.parse_args()


def main():
    args = parse_args()

    if os.path.exists(args.output):
        print(f"[INFO] Model already exist in '{args.output}'")
        return

    from ultralytics import YOLO

    print(f"[INFO] Downloading model: {args.model}")
    model = YOLO(args.model)

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    model.save(args.output)  # use YOLO.save() to correctly serialize the full object

    n_params = sum(p.numel() for p in model.model.parameters())
    print(f"[INFO] Guardado en: {args.output} ({n_params:,} parámetros)")


if __name__ == "__main__":
    main()