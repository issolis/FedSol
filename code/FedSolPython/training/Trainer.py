import json
import time
import torch
from datetime import datetime
import os

from FedSolPython.training.ModelBuilder import ModelBuilder
from FedSolPython.training.LossFactory import LossFactory
from FedSolPython.data.DataSetFactory import DatasetFactory



def _torch_load(path, map_location="cpu"):
    try:
        return torch.load(path, map_location=map_location, weights_only=False)
    except TypeError:
        return torch.load(path, map_location=map_location)

class Trainer:

    def __init__(self, config_path):
        self.config_path = config_path
        self.config = None
        self.model = None
        self.loss_fn = None
        self.optimizer = None

    def load_config(self):
        with open(self.config_path, "r") as f:
            self.config = json.load(f)

   
    def build_model(self):
        pt_path = self.config.get("model", {}).get("pt_path")

        if pt_path:
            print(f"[INFO] pt_path mode: loading base model from '{pt_path}'")
            self.model = _torch_load(pt_path)
            self.model.eval()

            weights = self.config.get("model", {}).get("weights", [])
            builder = ModelBuilder(self.config_path)
            builder.weights = weights
            builder.load_weights(self.model)
            
            print(f"[INFO] Server weights applied to model: {len(weights)} floats")
        else:
            builder = ModelBuilder(self.config_path)
            self.model = builder.build()

    def setup_training(self):
        training = self.config["training"]

        self.loss_fn = LossFactory.create(training["loss"])

        lr = training.get("lr", 0.001)

        if training["optimizer"] == "adam":
            self.optimizer = torch.optim.Adam(self.model.parameters(), lr=lr)

        elif training["optimizer"] == "sgd":
            self.optimizer = torch.optim.SGD(self.model.parameters(), lr=lr)

        else:
            raise ValueError("Unknown optimizer")

    def extract_weights(self):
        weights = []

        for param in self.model.parameters():
            weights.extend(param.data.view(-1).cpu().tolist())

        return weights

    def update_weights_in_json(self):
        weights = self.extract_weights()
        self.config["model"]["weights"] = weights

        with open(self.config_path, "w") as f:
            json.dump(self.config, f, indent=4)

        print(f"[INFO] Weights updated in JSON: {len(weights)}")

    def _generate_stats_path(self):
        now = datetime.now()

        date_str = now.strftime("%Y-%m-%d")
        time_str = now.strftime("%H-%M-%S")

        folder = f"output_client/stats/{date_str}"
        os.makedirs(folder, exist_ok=True)

        return f"{folder}/run_{time_str}.json"

    def save_stats(self, stats):
        path = self._generate_stats_path()

        with open(path, "w") as f:
            json.dump(stats, f, indent=4)

        print(f"[INFO] Training stats saved in: {path}")
    
    def _attach_timestamp(self, stats):
        stats["timestamp"] = datetime.now().isoformat()
    
    def printModel(self):
        if self.model is None:
            print("[WARNING] Model not built yet")
            return
 
        print("\n===== MODEL ARCHITECTURE =====\n")
 
        if not isinstance(self.model, torch.nn.Sequential):
            print(f"  {self.model.__class__.__name__} (non-sequential model)")
            print("================================\n")
            return
 
        for i, layer in enumerate(self.model):
            print(f"[Layer {i}] {layer.__class__.__name__}")
 
            if isinstance(layer, torch.nn.Conv2d):
                print(f"  in_channels: {layer.in_channels}")
                print(f"  out_channels: {layer.out_channels}")
                print(f"  kernel_size: {layer.kernel_size}")
                print(f"  stride: {layer.stride}")
                print(f"  padding: {layer.padding}")
                print(f"  bias: {layer.bias is not None}")
 
            elif isinstance(layer, torch.nn.Linear):
                print(f"  in_features: {layer.in_features}")
                print(f"  out_features: {layer.out_features}")
                print(f"  bias: {layer.bias is not None}")
 
            elif isinstance(layer, torch.nn.BatchNorm2d):
                print(f"  num_features: {layer.num_features}")
                print(f"  eps: {layer.eps}")
                print(f"  momentum: {layer.momentum}")
 
            elif isinstance(layer, torch.nn.MaxPool2d):
                print(f"  kernel_size: {layer.kernel_size}")
                print(f"  stride: {layer.stride}")
                print(f"  padding: {layer.padding}")
 
            elif isinstance(layer, torch.nn.Dropout):
                print(f"  p: {layer.p}")
 
            elif isinstance(layer, torch.nn.ReLU):
                print("  activation: ReLU")
 
            elif isinstance(layer, torch.nn.Sigmoid):
                print("  activation: Sigmoid")
 
            elif isinstance(layer, torch.nn.Tanh):
                print("  activation: Tanh")
 
            elif isinstance(layer, torch.nn.Softmax):
                print(f"  dim: {layer.dim}")
 
            elif isinstance(layer, torch.nn.Flatten):
                print("  Flatten layer")
 
            else:
                print("  [INFO] Unknown or custom layer")
 
            print()
 
        print("================================\n")
    
    def train(self):
        training = self.config["training"]
        dataset_cfg = self.config["dataset"]

        epochs = training.get("epochs", 5)
        batch_size = training.get("batch_size", 32)

        device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.model.to(device)

        loader = DatasetFactory.create(dataset_cfg, batch_size)

        stats = {
            "config_path": self.config_path,
            "device": str(device),
            "epochs": epochs,
            "batch_size": batch_size,
            "optimizer": training.get("optimizer"),
            "learning_rate": training.get("lr", 0.001),
            "loss_function": training.get("loss"),
            "total_samples": len(loader.dataset) if hasattr(loader, "dataset") else None,
            "epochs_stats": []
        }

        training_start = time.time()

        for epoch in range(epochs):
            self.model.train()
            total_loss = 0.0
            correct = 0
            total = 0

            epoch_start = time.time()

            for xb, yb in loader:
                xb, yb = xb.to(device), yb.to(device)

                self.optimizer.zero_grad()

                outputs = self.model(xb)
                loss = self.loss_fn(outputs, yb)

                loss.backward()
                self.optimizer.step()

                total_loss += loss.item()

                preds = torch.argmax(outputs, dim=1)
                correct += (preds == yb).sum().item()
                total += yb.size(0)

            avg_loss = total_loss / len(loader)
            accuracy = correct / total if total > 0 else 0.0
            epoch_time = time.time() - epoch_start

            epoch_stat = {
                "epoch": epoch + 1,
                "loss": avg_loss,
                "accuracy": accuracy,
                "correct_predictions": correct,
                "total_samples": total,
                "duration_seconds": epoch_time
            }

            stats["epochs_stats"].append(epoch_stat)

            print(
                f"[Epoch {epoch+1}] "
                f"Loss: {avg_loss:.4f} | "
                f"Accuracy: {accuracy:.4f} | "
                f"Time: {epoch_time:.2f}s"
            )

        stats["total_training_time_seconds"] = time.time() - training_start
        stats["final_loss"] = stats["epochs_stats"][-1]["loss"] if stats["epochs_stats"] else None
        stats["final_accuracy"] = stats["epochs_stats"][-1]["accuracy"] if stats["epochs_stats"] else None
        stats["weights_count"] = len(self.extract_weights())

        self.update_weights_in_json()
        self._attach_timestamp(stats)
        self.save_stats(stats)

        print("\n[TRAINING DONE]")

    def run(self):
        print("[INFO] Starting training pipeline...")

        self.load_config()
        self.build_model()
        self.setup_training()
        self.printModel()
        self.train()

        print("\n[INFO] Training finished.")