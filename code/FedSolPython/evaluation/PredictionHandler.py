import torch


class PredictionHandler:
    @staticmethod
    def predict(outputs, task_type, prediction_config):
        method = prediction_config.get("method")

        if task_type == "multiclass_classification":
            if method != "argmax":
                raise ValueError(
                    "multiclass_classification requires prediction method 'argmax'"
                )
            return torch.argmax(outputs, dim=1)

        if task_type == "binary_classification":
            if method == "argmax":
                return torch.argmax(outputs, dim=1)

            if method == "threshold":
                from_logits = prediction_config.get("from_logits", True)
                threshold = prediction_config.get("threshold", 0.5)

                preds = outputs
                if preds.dim() > 1 and preds.size(1) == 1:
                    preds = preds.squeeze(1)

                if from_logits:
                    preds = torch.sigmoid(preds)

                return (preds >= threshold).long()

            raise ValueError(
                "binary_classification supports 'argmax' or 'threshold'"
            )

        if task_type in ["regression", "multivariate_regression"]:
            if method != "direct":
                raise ValueError(
                    f"{task_type} requires prediction method 'direct'"
                )
            return outputs

        raise ValueError(f"Unsupported task type: {task_type}")