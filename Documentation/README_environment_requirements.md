# FedSol Secure — Environment and Dependency Requirements

This document describes the software environment required to compile and run FedSol Secure. The dependency list was derived from the repository structure, the `Makefile`, the C++ includes, and the Python imports under `code/FedSolPython`.

The project does not include a definitive `requirements.txt`, so this file separates dependencies by role:

1. System and compiler requirements.
2. C++ dependencies.
3. Python base dependencies.
4. Optional dependencies for classic `.npz` experiments.
5. Additional dependencies for YOLO experiments.
6. Directory and runtime requirements.
7. Installation examples.

---

## 1. System overview

FedSol Secure is a hybrid C++/Python project.

The C++ layer builds:

```text
server_app
client_app
```

The Python layer is executed from C++ through commands such as:

```bash
PYTHONPATH=code python3 -m FedSolPython.scripts.train <configClient.json>
```

```bash
PYTHONPATH=code python3 -m FedSolPython.scripts.evaluate <config.json>
```

```bash
PYTHONPATH=code python3 -m FedSolPython.scripts.load_pt_weights <config.json>
```

Because `PYTHONPATH=code` is used, the recommended execution location is the repository root.

---

## 2. Operating system

The project is intended for Linux.

The C++ code uses POSIX/Linux headers and APIs such as:

```text
arpa/inet.h
sys/socket.h
sys/stat.h
sys/time.h
sys/wait.h
unistd.h
```

Recommended environments:

```text
Ubuntu 20.04
Ubuntu 22.04
Jetson Linux / L4T compatible with Ubuntu userspace
```

Windows is not a native target unless using WSL or a compatible Linux environment.

---

## 3. C++ compiler requirements

The `Makefile` uses:

```makefile
CXX = g++
CXXFLAGS_COMMON = -std=c++17 -pthread -Icode/headers -O3 -fopenmp
CXXFLAGS_SERVER = $(CXXFLAGS_COMMON) -mavx2
CXXFLAGS_CLIENT = $(CXXFLAGS_COMMON)
```

Required tools:

| Requirement | Reason |
|---|---|
| `g++` | Compiles C++ sources |
| `make` | Runs the build rules |
| C++17 support | The project compiles with `-std=c++17` |
| pthread support | The code uses threads |
| OpenMP / `libgomp` | The build uses `-fopenmp` |
| `nlohmann/json.hpp` | JSON parsing in C++ |
| AVX2 support for server | The server build uses `-mavx2` |

Install base build tools on Ubuntu:

```bash
sudo apt update
sudo apt install -y build-essential make g++ nlohmann-json3-dev libgomp1
```

---

## 4. AVX2 requirement for the server

The server is compiled with:

```makefile
-mavx2
```

The FedAvg implementation uses AVX2 intrinsics such as:

```text
_mm256_loadu_ps
_mm256_set1_ps
_mm256_storeu_ps
```

This means `server_app` should be compiled and executed on a CPU that supports AVX2.

Check AVX2 support:

```bash
grep -m1 avx2 /proc/cpuinfo
```

If this prints a line containing `avx2`, the CPU supports it.

Important distinction:

- The server build uses `-mavx2`.
- The client build does not use `-mavx2` in the current `Makefile`.

This is useful when clients run on devices such as Jetson Nano, while the server runs on a laptop or desktop CPU.

If the server must run on hardware without AVX2, the `Makefile` and FedAvg implementation would need to be adjusted.

---

## 5. C++ external dependency: nlohmann JSON

The code includes:

```cpp
#include <nlohmann/json.hpp>
```

Install with:

```bash
sudo apt install -y nlohmann-json3-dev
```

If the dependency is missing, compilation fails with:

```text
fatal error: nlohmann/json.hpp: No such file or directory
```

On systems where `apt` is not available, the single-header `json.hpp` can be placed in an include path such as:

```text
/usr/local/include/nlohmann/json.hpp
```

or another path visible to the compiler.

---

## 6. Python version

The code invokes:

```bash
python3
```

Recommended versions:

```text
Python 3.8+
```

For modern desktop/laptop environments, Python 3.10 works well.

For Jetson Nano environments, Python 3.8 is common depending on the Jetson Linux/L4T distribution.

Check Python:

```bash
python3 --version
```

---

## 7. Python package structure

FedSol Secure expects the repository package to be importable as:

```python
FedSolPython
```

This is achieved by running scripts with:

```bash
PYTHONPATH=code
```

Example:

```bash
PYTHONPATH=code python3 -m FedSolPython.scripts.train input_client/configClient.json
```

Do not run Python scripts from random directories unless `PYTHONPATH` is set correctly and relative paths still resolve.

---

## 8. Python dependencies observed in the code

The Python code imports these external libraries:

| Package | Used for |
|---|---|
| `numpy` | Loading `.npz`, splitting datasets, metrics support |
| `torch` | PyTorch models, training, tensors, DataLoader |
| `sklearn` / `scikit-learn` | Classification and regression metrics |
| `ultralytics` | YOLO model loading, training, prediction, evaluation |

The Python code also uses standard-library modules:

```text
argparse, copy, datetime, glob, hashlib, json, math, os, pathlib,
shutil, sys, tarfile, time, warnings, webbrowser
```

These standard modules do not require pip installation.

---

## 9. Minimal Python requirements for `.npz` experiments

For classic classification or regression experiments using `.npz` datasets, install:

```bash
python3 -m pip install numpy torch scikit-learn
```

These are needed for:

- `DataSetFactory.py`
- `ModelBuilder.py`
- `Trainer.py`
- `LossFactory.py`
- `Evaluator.py`
- `ClassificationMetrics.py`
- `RegressionMetrics.py`

### 9.1 PyTorch note

PyTorch installation depends heavily on the hardware and operating system.

On a normal CPU-only Linux machine, a typical installation may be:

```bash
python3 -m pip install torch --index-url https://download.pytorch.org/whl/cpu
```

On CUDA-enabled systems, use the PyTorch build that matches the CUDA version.

On Jetson devices, use a Jetson-compatible PyTorch build. Installing a generic PyPI wheel may fail or produce runtime errors.

---

## 10. Additional Python requirements for YOLO experiments

For YOLO mode, install:

```bash
python3 -m pip install ultralytics
```

YOLO mode also requires PyTorch, NumPy, and usually additional packages pulled automatically by Ultralytics, such as OpenCV and related utilities.

Minimum practical set:

```bash
python3 -m pip install numpy torch scikit-learn ultralytics
```

On Jetson devices, install PyTorch first using a Jetson-compatible method, then install Ultralytics in a way compatible with that PyTorch version.

---

## 11. Recommended virtual environment

Create a virtual environment:

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install --upgrade pip
```

Install dependencies for `.npz` mode:

```bash
python3 -m pip install numpy torch scikit-learn
```

Install dependencies for YOLO mode:

```bash
python3 -m pip install numpy torch scikit-learn ultralytics
```

Verify imports:

```bash
python3 - <<'PY'
import numpy
import torch
import sklearn
print("numpy:", numpy.__version__)
print("torch:", torch.__version__)
print("sklearn: ok")
PY
```

For YOLO:

```bash
python3 - <<'PY'
from ultralytics import YOLO
print("ultralytics: ok")
PY
```

---

## 12. Build instructions

From the repository root:

```bash
make clean
make
```

This builds:

```text
server_app
client_app
```

Build only the server:

```bash
make server_app
```

Build only the client:

```bash
make client_app
```

Run through Makefile targets:

```bash
make run_server CONFIG=configTest/test3/config.json
```

```bash
make run_client CONFIG=input_client/configClient.json
```

Or run directly:

```bash
./server_app configTest/test3/config.json
```

```bash
./client_app input_client/configClient.json
```

---

## 13. Runtime `.env` requirement

The server requires a `.env` file in the repository root.

Required key:

```env
SERVER_PASSWORD_HASH=<sha256_hash>
```

The server loads this value at runtime. If it is missing, authentication initialization fails.

Generate a SHA-256 hash for a password:

```bash
printf '%s' 'federated123' | sha256sum
```

Then place the hash in `.env`:

```env
SERVER_PASSWORD_HASH=<output_hash_without_filename>
```

Clients still store the plaintext password in `configClient.json`:

```json
"password": "federated123"
```

The server hashes the received password and compares it with the stored hash.

---

## 14. Required runtime directories

The code creates some output directories automatically, but it is good practice to keep this structure:

```text
input_server/
input_client/
output_server/
output_client/
```

Common server-side input files:

```text
input_server/dataset_train.npz
input_server/dataset_test.npz
input_server/model.pt
```

Common client-side input files:

```text
input_client/configClient.json
input_client/dataset_train.npz
input_client/dataset_test.npz
input_client/model.pt
```

For YOLO server datasets:

```text
input_server/dataset/
├── train/
│   ├── images/
│   └── labels/
└── val/
    ├── images/
    └── labels/
```

For YOLO clients receiving datasets from the server:

```text
input_client/dataset.tar.gz
input_client/dataset_local/
```

The unpacking script can create and update `dataset_local` automatically.

---

## 15. Expected output directories

During execution, FedSol Secure writes logs, stats, models, and reports.

Common outputs:

```text
output_server/logs/server.log
output_server/models/model_<timestamp>.pt
output_server/globalResults/
output_server/globalResults/stats/
output_client/logs/client_<id>.log
output_client/stats/<date>/run_<time>.json
output_client/yolo_runs/
```

If a directory is missing and the code path does not create it automatically, create it manually:

```bash
mkdir -p output_server/logs output_server/models output_server/globalResults
mkdir -p output_client/logs output_client/stats output_client/yolo_runs
```

---

## 16. Dependencies by feature

### 16.1 Compile only

Required:

```text
g++
make
nlohmann-json3-dev
libgomp1
```

### 16.2 Run basic server/client networking

Required:

```text
Linux/POSIX environment
.env with SERVER_PASSWORD_HASH
valid config.json and configClient.json
```

### 16.3 Train `.npz` models

Required Python packages:

```text
numpy
torch
scikit-learn
```

### 16.4 Use generic `.pt` models

Required Python packages:

```text
torch
numpy
scikit-learn
```

Additional requirement:

```text
The .pt model must be a full model object or a compatible loadable object.
The architecture must map to supported FedSol layers.
```

### 16.5 Train YOLO models

Required Python packages:

```text
torch
numpy
scikit-learn
ultralytics
```

Additional requirements:

```text
YOLO-compatible .pt file
YOLO dataset directory structure
sufficient memory for selected batch_size and imgsz
```

---

## 17. Python modules and their external imports

The following table summarizes important Python files and external packages.

| File | External packages |
|---|---|
| `data/DataSetFactory.py` | `numpy`, `torch` |
| `training/ModelBuilder.py` | `torch` |
| `training/Trainer.py` | `torch` |
| `training/YOLOTrainer.py` | `torch`, `ultralytics` |
| `training/LossFactory.py` | `torch` |
| `evaluation/Evaluator.py` | `torch`, `ultralytics` |
| `evaluation/EvaluatorYolo.py` | `numpy`, `torch`, `ultralytics` |
| `evaluation/ClassificationMetrics.py` | `sklearn` |
| `evaluation/RegressionMetrics.py` | `sklearn` |
| `scripts/split_dataset.py` | `numpy` |
| `scripts/get_total_samples.py` | `numpy` |
| `scripts/load_pt_arch.py` | `torch`, optionally `ultralytics` |
| `scripts/load_pt_weights.py` | `torch`, optionally `ultralytics` |
| `scripts/get_pt_arch_hash.py` | `torch`, optionally `ultralytics` |
| `scripts/build_and_save.py` | `torch`, optionally `ultralytics` |
| `scripts_yolo/init_yolo_model.py` | `torch`, `ultralytics` |

Even when `ultralytics` is listed as optional for some `.pt` scripts, it is required for YOLO experiments.

---

## 18. Hardware considerations

### 18.1 Server

Recommended server hardware:

- x86_64 CPU with AVX2 support;
- enough RAM to hold model weights and datasets;
- stable network connection to clients.

### 18.2 Clients

Clients can run on less powerful devices, but local training requirements depend on the model.

For small `.npz` CNNs:

- CPU-only execution can work.
- Batch size can be moderate.

For YOLO:

- GPU is highly recommended.
- Small devices may require low `batch_size` and small `imgsz`.
- Jetson Nano experiments usually need careful PyTorch and Ultralytics version compatibility.

---

## 19. Jetson Nano notes

Jetson Nano environments are sensitive to package versions.

Recommended approach:

1. Use a Jetson-compatible PyTorch build.
2. Verify that `import torch` works before installing higher-level packages.
3. Install NumPy versions compatible with the selected PyTorch and Python version.
4. Install Ultralytics only after PyTorch is stable.
5. Use small YOLO settings:

```json
"batch_size": 1
```

or:

```json
"batch_size": 2
```

and:

```json
"imgsz": 320
```

If the process is killed or becomes extremely slow, reduce batch size, reduce image size, disable unnecessary services, or increase swap.

---

## 20. Quick installation examples

### 20.1 Ubuntu CPU-only classic `.npz` setup

```bash
sudo apt update
sudo apt install -y build-essential make g++ nlohmann-json3-dev libgomp1 python3 python3-venv python3-pip

python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install --upgrade pip
python3 -m pip install numpy scikit-learn
python3 -m pip install torch --index-url https://download.pytorch.org/whl/cpu

make clean
make
```

### 20.2 Ubuntu YOLO setup

```bash
sudo apt update
sudo apt install -y build-essential make g++ nlohmann-json3-dev libgomp1 python3 python3-venv python3-pip

python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install --upgrade pip
python3 -m pip install numpy scikit-learn torch ultralytics

make clean
make
```

### 20.3 Client-only build

Useful for devices that should not compile the AVX2 server:

```bash
sudo apt update
sudo apt install -y build-essential make g++ nlohmann-json3-dev libgomp1 python3 python3-pip
make client_app
```

---

## 21. Validation commands

Check C++ build:

```bash
make clean
make
```

Check Python package imports:

```bash
PYTHONPATH=code python3 - <<'PY'
import numpy
import torch
import sklearn
from FedSolPython.training.ModelBuilder import ModelBuilder
print("Classic environment: ok")
PY
```

Check YOLO import:

```bash
PYTHONPATH=code python3 - <<'PY'
from ultralytics import YOLO
print("YOLO environment: ok")
PY
```

Check sample counting for `.npz`:

```bash
PYTHONPATH=code python3 -m FedSolPython.scripts.get_total_samples input_client/configClient.json
```

Check `.pt` architecture hash:

```bash
PYTHONPATH=code python3 -m FedSolPython.scripts.get_pt_arch_hash input_client/model.pt
```

---

## 22. Common errors and causes

### 22.1 `nlohmann/json.hpp: No such file or directory`

Cause:

```text
nlohmann-json3-dev is not installed.
```

Fix:

```bash
sudo apt install -y nlohmann-json3-dev
```

### 22.2 `SERVER_PASSWORD_HASH not found in .env`

Cause:

```text
The server cannot find .env or the key is missing.
```

Fix:

Create `.env` in the repository root:

```env
SERVER_PASSWORD_HASH=<sha256_hash>
```

### 22.3 `Unsupported dataset type`

Cause:

```text
The dataset type is not implemented for that path.
```

For the classic `DatasetFactory`, supported type is:

```json
"type": "npz"
```

For YOLO, training is routed through `YOLOTrainer` when:

```json
"type": "yolo"
```

### 22.4 `Key 'X' not found in NPZ`

Cause:

```text
The .npz file does not contain an array named X.
```

Fix:

Create the dataset with:

```python
np.savez("dataset_train.npz", X=X, y=y)
```

### 22.5 `PT handshake failed` or `HASH_INVALID`

Possible causes:

- Server and client `.pt` files do not have the same architecture.
- YOLO `nc` differs between server and client.
- The wrong model file path is configured.
- One side loaded a different model version.

Fix:

Use matching `model.pt` files and matching `dataset.nc` values.

### 22.6 Weight count mismatch during aggregation

Example:

```text
Client sent N weights, expected M.
```

Possible causes:

- Client and server model architectures differ.
- YOLO `nc` differs between server and client.
- Generic `.pt` model has unsupported layers.
- BatchNorm buffers are included on one side but not the other.
- An old config still contains stale weights.

Fix:

Regenerate matching configs, use matching `.pt` files, and ensure all clients use the same model mode.

---

## 23. Recommended environment checklist

Before running experiments:

1. Confirm Linux/POSIX environment.
2. Confirm `g++`, `make`, and C++17 support.
3. Confirm `nlohmann/json.hpp` is installed.
4. Confirm server CPU supports AVX2, or compile only the client on non-AVX2 devices.
5. Confirm Python can import `numpy`, `torch`, and `sklearn`.
6. Confirm Python can import `ultralytics` for YOLO experiments.
7. Confirm `.env` exists in the repository root.
8. Confirm `PYTHONPATH=code` works.
9. Confirm dataset files or directories exist.
10. Confirm model `.pt` paths exist when using `pt_path` mode.
11. Confirm server and clients use compatible model modes.
12. Confirm YOLO `nc` and `names` are consistent across server and clients.
