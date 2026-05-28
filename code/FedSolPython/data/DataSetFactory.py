import numpy as np
import torch
from torch.utils.data import DataLoader, TensorDataset


class DatasetFactory:

    @staticmethod
    def create(dataset_cfg, batch_size, train=True):
        if dataset_cfg["type"] != "npz":
            raise ValueError("Unsupported dataset type")

        path = dataset_cfg["train_path"] if train else dataset_cfg["test_path"]
        shuffle = train

        target_dtype = dataset_cfg.get("target_dtype", "long")

        data = np.load(path)

        X = torch.from_numpy(data["X"]).float()

        if target_dtype == "long":
            y = torch.from_numpy(data["y"]).long()
        elif target_dtype == "float":
            y = torch.from_numpy(data["y"]).float()
        else:
            raise ValueError(f"Unsupported target_dtype: {target_dtype}")

        dataset = TensorDataset(X, y)

        return DataLoader(
            dataset,
            batch_size=batch_size,
            shuffle=shuffle,
            num_workers=0,      
            pin_memory=True  
        )