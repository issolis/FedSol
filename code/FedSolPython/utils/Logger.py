import os
from datetime import datetime


class Logger:
    _log_file = None

    @staticmethod
    def configure(log_file):
        Logger._log_file = log_file

        # Crear carpeta si no existe
        folder = os.path.dirname(log_file)
        if folder:
            os.makedirs(folder, exist_ok=True)

    @staticmethod
    def _write(level, message):
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_msg = f"[{timestamp}] [{level}] {message}"

        # Archivo (si está configurado)
        if Logger._log_file:
            with open(Logger._log_file, "a") as f:
                f.write(log_msg + "\n")

    @staticmethod
    def info(message):
        Logger._write("INFO", message)

    @staticmethod
    def warning(message):
        Logger._write("WARNING", message)

    @staticmethod
    def error(message):
        Logger._write("ERROR", message)