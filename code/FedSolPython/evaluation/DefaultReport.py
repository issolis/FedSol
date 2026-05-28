import json
import os
from datetime import datetime


class DefaultReport:

    @staticmethod
    def generate(results, params):
        # ── Console output ────────────────────────────────────────────────────
        if params.get("print_to_console", True):
            print("[EVALUATION RESULTS]")
            for key, value in results.items():
                if key not in ("curves", "_meta"):
                    print(f"{key}: {value}")

        if not params.get("save_results", False):
            return

        # ── Paths ─────────────────────────────────────────────────────────────
        base_dir           = params.get("base_dir", "output_server/globalResults")
        use_timestamp_path = params.get("use_timestamp_path", True)

        if use_timestamp_path:
            now       = datetime.now()
            day_folder = now.strftime("%Y-%m-%d")
            stem      = now.strftime("%Y-%m-%d_%H-%M-%S")
            json_path = os.path.join(base_dir, day_folder, stem + ".json")
            html_path = os.path.join(base_dir, day_folder, stem + "_report.html")
        else:
            json_path = params.get("results_path", "results/evaluation.json")
            html_path = json_path.replace(".json", "_report.html")

        os.makedirs(os.path.dirname(json_path), exist_ok=True)

        # ── Save JSON (scalar metrics only) ───────────────────────────────────
        scalar_results = {
            k: v for k, v in results.items()
            if k not in ("curves", "_meta")
        }
        scalar_results["saved_at"] = json_path

        with open(json_path, "w") as f:
            json.dump(scalar_results, f, indent=4)

        # ── HTML report (only when curve data is available) ───────────────────
        if "curves" in results:
            from FedSolPython.evaluation.DefaultReportYolo import DefaultReportYolo
            DefaultReportYolo.generate(results, params, html_path)