class TaskValidator:
    VALID_METRICS = {
        "binary_classification": {
            "loss",
            "accuracy",
            "precision",
            "recall",
            "f1",
            "confusion_matrix",
        },
        "multiclass_classification": {
            "loss",
            "accuracy",
            "precision",
            "recall",
            "f1",
            "confusion_matrix",
        },
        "regression": {
            "loss",
            "mse",
            "mae",
            "rmse",
        },
        "multivariate_regression": {
            "loss",
            "mse",
            "mae",
            "rmse",
        },
        
        "object_detection": {
            "map50",
            "map50_95",
            "precision",
            "recall",
        }
    }

    @staticmethod
    def validate(task_type, metrics_config):
        if task_type not in TaskValidator.VALID_METRICS:
            raise ValueError(f"Unsupported task type: {task_type}")

        valid = TaskValidator.VALID_METRICS[task_type]

        for metric_cfg in metrics_config:
            if isinstance(metric_cfg, str):
                metric_name = metric_cfg
            else:
                metric_name = metric_cfg["name"]

            if metric_name not in valid:
                raise ValueError(
                    f"Metric '{metric_name}' is not valid for task_type '{task_type}'"
                )