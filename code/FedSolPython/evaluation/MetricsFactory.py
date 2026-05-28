from FedSolPython.evaluation.ClassificationMetrics import ClassificationMetrics
from FedSolPython.evaluation.RegressionMetrics import RegressionMetrics


class MetricsFactory:
    @staticmethod
    def compute(task_type, metrics_config, y_true, y_pred, avg_loss=None):
        results = {}

        for metric_cfg in metrics_config:
            if isinstance(metric_cfg, str):
                metric_cfg = {"name": metric_cfg}

            metric_name = metric_cfg["name"]

            if metric_name == "loss":
                if avg_loss is None:
                    raise ValueError(
                        "Metric 'loss' was requested but no loss was computed"
                    )
                results["loss"] = float(avg_loss)
                continue

            if task_type in ["binary_classification", "multiclass_classification"]:
                value = ClassificationMetrics.compute(
                    metric_name=metric_name,
                    y_true=y_true,
                    y_pred=y_pred,
                    metric_cfg=metric_cfg,
                )

            elif task_type in ["regression", "multivariate_regression"]:
                value = RegressionMetrics.compute(
                    metric_name=metric_name,
                    y_true=y_true,
                    y_pred=y_pred,
                    metric_cfg=metric_cfg,
                )

            else:
                raise ValueError(f"Unsupported task type: {task_type}")

            results[MetricsFactory._build_metric_key(metric_name, metric_cfg)] = value

        return results

    @staticmethod
    def _build_metric_key(metric_name, metric_cfg):
        average = metric_cfg.get("average")
        aggregation = metric_cfg.get("aggregation")

        if average is not None:
            return f"{metric_name}_{average}"

        if aggregation is not None and aggregation != "global":
            return f"{metric_name}_{aggregation}"

        return metric_name