import sys

from FedSolPython.evaluation.Evaluator import Evaluator


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 -m FedSolPython.scripts.evaluate <config_path>")
        return 1

    config_path = sys.argv[1]

    try:
        evaluator = Evaluator(config_path)
        evaluator.run()
        return 0

    except Exception as e:
        print(f"[EVALUATION ERROR] {str(e)}")
        return 1


if __name__ == "__main__":
    exit(main())