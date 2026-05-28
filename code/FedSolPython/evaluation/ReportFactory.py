from FedSolPython.evaluation.DefaultReport import DefaultReport


class ReportFactory:

    @staticmethod
    def generate(report_config, results):
        name = report_config.get("name", "default_report")
        params = report_config.get("params", {})

        if name == "default_report":
            DefaultReport.generate(results, params)
            return

        raise ValueError(f"Unsupported report function: {name}")