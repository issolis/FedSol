import math
from sklearn.metrics import mean_squared_error, mean_absolute_error


class RegressionMetrics:
    @staticmethod
    def compute(metric_name, y_true, y_pred, metric_cfg):
        aggregation = metric_cfg.get("aggregation", "global")

        if aggregation == "global":
            y_true_flat = y_true.reshape(-1)
            y_pred_flat = y_pred.reshape(-1)

            if metric_name == "mse":
                return float(mean_squared_error(y_true_flat, y_pred_flat))

            if metric_name == "mae":
                return float(mean_absolute_error(y_true_flat, y_pred_flat))

            if metric_name == "rmse":
                mse = mean_squared_error(y_true_flat, y_pred_flat)
                return float(math.sqrt(mse))

        elif aggregation == "per_output":
            if len(y_true.shape) == 1:
                y_true = y_true.reshape(-1, 1)
                y_pred = y_pred.reshape(-1, 1)

            results = []

            for col in range(y_true.shape[1]):
                true_col = y_true[:, col]
                pred_col = y_pred[:, col]

                if metric_name == "mse":
                    results.append(float(mean_squared_error(true_col, pred_col)))

                elif metric_name == "mae":
                    results.append(float(mean_absolute_error(true_col, pred_col)))

                elif metric_name == "rmse":
                    mse = mean_squared_error(true_col, pred_col)
                    results.append(float(math.sqrt(mse)))

                else:
                    raise ValueError(f"Unsupported regression metric: {metric_name}")

            return results

        raise ValueError(
            f"Unsupported aggregation '{aggregation}' for regression metric '{metric_name}'"
        )