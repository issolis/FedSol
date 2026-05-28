from sklearn.metrics import (
    accuracy_score,
    precision_score,
    recall_score,
    f1_score,
    confusion_matrix,
)


class ClassificationMetrics:
    @staticmethod
    def compute(metric_name, y_true, y_pred, metric_cfg):
        average = metric_cfg.get("average", "macro")
        zero_division = metric_cfg.get("zero_division", 0)

        if metric_name == "accuracy":
            return float(accuracy_score(y_true, y_pred))

        if metric_name == "precision":
            return float(
                precision_score(
                    y_true,
                    y_pred,
                    average=average,
                    zero_division=zero_division
                )
            )

        if metric_name == "recall":
            return float(
                recall_score(
                    y_true,
                    y_pred,
                    average=average,
                    zero_division=zero_division
                )
            )

        if metric_name == "f1":
            return float(
                f1_score(
                    y_true,
                    y_pred,
                    average=average,
                    zero_division=zero_division
                )
            )

        if metric_name == "confusion_matrix":
            return confusion_matrix(y_true, y_pred).tolist()

        raise ValueError(f"Unsupported classification metric: {metric_name}")