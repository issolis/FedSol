import torch.nn as nn


class LossFactory:

    @staticmethod
    def create(loss_config):
        if isinstance(loss_config, str):
            name = loss_config.strip().lower()
            params = {}

        elif isinstance(loss_config, dict):
            if "name" not in loss_config:
                raise ValueError("Loss config must contain 'name'")

            name = loss_config["name"].strip().lower()
            params = loss_config.get("params", {})

            if not isinstance(params, dict):
                raise TypeError("Loss params must be a dictionary")

        else:
            raise TypeError("Loss config must be a string or a dictionary")

        if name == "cross_entropy":
            return nn.CrossEntropyLoss(**params)

        elif name == "mse":
            return nn.MSELoss(**params)

        elif name == "mae":
            return nn.L1Loss(**params)

        elif name == "bce":
            return nn.BCELoss(**params)

        elif name in ["bce_logits", "bce_with_logits"]:
            return nn.BCEWithLogitsLoss(**params)
        
        elif name == "yolo_default":
            # YOLO maneja su propia función de pérdida internamente durante model.train().
            # Este valor en el config es solo informativo — no se usa en Evaluator ni Trainer.
            return None

        else:
            raise ValueError(f"Unknown loss: {name}")