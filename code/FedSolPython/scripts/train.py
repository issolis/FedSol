from FedSolPython.training.Trainer import Trainer
import sys
import json


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 train.py <config_path>")
        return 1

    config_path = sys.argv[1]

    try:
        with open(config_path, "r") as f:
            config = json.load(f)
 
        dataset_type = config.get("dataset", {}).get("type", "npz")
 
        if dataset_type == "yolo":
            from FedSolPython.training.YOLOTrainer import YOLOTrainer
            trainer = YOLOTrainer(config_path)
        else:
            from FedSolPython.training.Trainer import Trainer
            trainer = Trainer(config_path)
 
        trainer.run()
        return 0

    except Exception as e:
        print(f"[ERROR] Training failed: {e}")
        return 1


if __name__ == "__main__":
    exit_code = main()
    sys.exit(exit_code)