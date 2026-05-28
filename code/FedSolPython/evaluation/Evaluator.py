import json
import os
import glob
import torch

from FedSolPython.training.LossFactory import LossFactory
from FedSolPython.evaluation.TaskValidator import TaskValidator
from FedSolPython.evaluation.ReportFactory import ReportFactory
from FedSolPython.evaluation.EvaluatorYolo import EvaluatorYolo, load_model_robust


class Evaluator:
    def __init__(self, config_path, model=None):
        self.config_path = config_path
        self.model       = model
        self.config      = None
        self.loss_fn     = None
        self.device      = None

    # ── Config ────────────────────────────────────────────────────────────────

    def load_config(self):
        with open(self.config_path, "r") as f:
            self.config = json.load(f)

    def _task_type(self) -> str:
        return self.config.get("evaluation", {}).get("task_type", "")

    def _is_yolo(self) -> bool:
        return self._task_type() == "object_detection"

    # ── Model loading ─────────────────────────────────────────────────────────

    def build_model(self):
        if self.model is not None:
            return

        pt_files = glob.glob("output_server/models/*.pt")
        if not pt_files:
            raise FileNotFoundError(
                "[Evaluator] No .pt model found in output_server/models/. "
                "Run at least one training round before evaluating."
            )

        latest_pt = max(pt_files, key=os.path.getmtime)
        print(f"[Evaluator] Loading model from: {latest_pt}")

        raw = load_model_robust(latest_pt)

        if self._is_yolo():
            from ultralytics import YOLO
            import copy

            if isinstance(raw, dict):
                pt_path = self.config.get("model", {}).get("pt_path")
                if not pt_path:
                    raise ValueError(
                        "[Evaluator] .pt contains a state_dict but 'model.pt_path' "
                        "is not set in config."
                    )
                print(f"[Evaluator] state_dict detected — rebuilding from: {pt_path}")
                from ultralytics.nn.tasks import DetectionModel
                base      = load_model_robust(pt_path)
                nc        = self.config.get("dataset", {}).get("nc", None)
                base_nc   = getattr(base.model, "nc", None) or base.model.model[-1].nc
                target_nc = nc if nc is not None else base_nc
                if target_nc != base_nc:
                    yaml_cfg = copy.deepcopy(base.model.yaml)
                    yaml_cfg["nc"] = target_nc
                    det = DetectionModel(yaml_cfg, ch=3, nc=target_nc)
                    base.model = det
                base.model.load_state_dict(raw, strict=False)
                self.model = base
            elif isinstance(raw, YOLO):
                self.model = raw
            else:
                self.model = YOLO(latest_pt)

            self.model.model.eval()

        else:
            if isinstance(raw, dict):
                raise ValueError(
                    "[Evaluator] .pt contains a state_dict but no architecture."
                )
            self.model = raw
            self.model.eval()

    # ── Setup ─────────────────────────────────────────────────────────────────

    def setup_loss(self):
        loss_cfg = self.config.get("training", {}).get("loss")
        self.loss_fn = LossFactory.create(loss_cfg) if loss_cfg else None

    def setup_device(self):
        device_name = self.config.get("training", {}).get("device", "cpu")
        self.device = (
            torch.device("cuda")
            if device_name == "cuda" and torch.cuda.is_available()
            else torch.device("cpu")
        )
        if not self._is_yolo():
            self.model.to(self.device)

    def validate_evaluation_config(self):
        evaluation_cfg = self.config["evaluation"]
        TaskValidator.validate(evaluation_cfg["task_type"], evaluation_cfg["metrics"])

    # ── Evaluation ────────────────────────────────────────────────────────────

    def evaluate(self) -> dict:
        if self._is_yolo():
            yolo_evaluator = EvaluatorYolo(self.model, self.config)
            return yolo_evaluator.evaluate()
        return self._run_classic_evaluation()

    def _run_classic_evaluation(self) -> dict:
        from FedSolPython.data.DataSetFactory import DatasetFactory
        from FedSolPython.evaluation.PredictionHandler import PredictionHandler
        from FedSolPython.evaluation.MetricsFactory import MetricsFactory

        evaluation_cfg    = self.config["evaluation"]
        task_type         = evaluation_cfg["task_type"]
        prediction_config = evaluation_cfg["prediction_config"]
        metrics_config    = evaluation_cfg["metrics"]
        dataset_cfg       = self.config["dataset"]
        batch_size        = self.config.get("training", {}).get("batch_size", 32)

        loader = DatasetFactory.create(dataset_cfg, batch_size, train=False)
        self.model.eval()

        total_loss    = 0.0
        total_samples = 0
        y_true_all    = []
        y_pred_all    = []

        with torch.no_grad():
            for xb, yb in loader:
                xb = xb.to(self.device)
                yb = yb.to(self.device)
                outputs = self.model(xb)

                if self.loss_fn is not None:
                    total_loss += self.loss_fn(outputs, yb).item() * xb.size(0)

                preds = PredictionHandler.predict(
                    outputs=outputs,
                    task_type=task_type,
                    prediction_config=prediction_config,
                )
                y_true_all.append(yb.detach().cpu())
                y_pred_all.append(preds.detach().cpu())
                total_samples += xb.size(0)

        y_true   = torch.cat(y_true_all, dim=0).numpy()
        y_pred   = torch.cat(y_pred_all, dim=0).numpy()
        avg_loss = (total_loss / total_samples) if (self.loss_fn and total_samples > 0) else None

        results = MetricsFactory.compute(
            task_type=task_type,
            metrics_config=metrics_config,
            y_true=y_true,
            y_pred=y_pred,
            avg_loss=avg_loss,
        )
        results["total_samples"] = int(total_samples)
        return results

    # ── Report & run ──────────────────────────────────────────────────────────

    def report(self, results):
        report_cfg = self.config["evaluation"].get("report_function", {
            "name": "default_report",
            "params": {},
        })
        ReportFactory.generate(report_cfg, results)

    def run(self):
        self.load_config()
        self.build_model()
        self.setup_loss()
        self.setup_device()
        self.validate_evaluation_config()

        results = self.evaluate()
        self.report(results)

        return results