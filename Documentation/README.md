# FedSol Secure — Project README

FedSol Secure is a C++/Python federated learning system designed to coordinate distributed training rounds between one central server and multiple client nodes. The project combines a C++ networking and orchestration layer with a Python training and evaluation layer. It supports classic tensor-based learning with `.npz` datasets and object detection experiments with Ultralytics YOLO models.

The repository also includes an optional defense mechanism against poisoned or abnormal client updates. When enabled, the server applies norm clipping to client model updates before federated averaging.

---

## 1. Repository purpose

FedSol Secure is intended for experiments where several devices train a model locally and send updated weights to a server. The server aggregates those weights with FedAvg and redistributes the new global model in the next round.

The system can be used for:

- multiclass or binary classification with NumPy `.npz` datasets;
- regression or multivariate regression with `.npz` datasets;
- object detection with YOLO datasets and `.pt` model files;
- experiments with poisoned clients and backdoor-defense logic;
- timing and performance measurements in heterogeneous devices such as laptops, Jetson Nano boards, or Raspberry Pi nodes.

---

## 2. High-level architecture

FedSol Secure has two main executables:

```bash
./server_app <server_config.json>
./client_app <configClient.json>
```

The server performs the following tasks:

1. Loads the global configuration from `config.json`.
2. Loads or reconstructs the global model.
3. Opens a TCP socket and waits for clients.
4. Authenticates clients using a password hash stored in `.env`.
5. Verifies that each client is compatible with the server model.
6. Sends the current global weights to each client.
7. Receives trained weights from the clients.
8. Aggregates the weights with FedAvg.
9. Optionally applies the backdoor-defense filter before aggregation.
10. Exports the resulting global model.
11. Runs evaluation reports when requested from the server menu.

Each client performs the following tasks:

1. Loads its local `configClient.json`.
2. Connects to the server using `server_ip` and `port`.
3. Authenticates with the shared password.
4. Validates model compatibility with the server.
5. Waits for server commands.
6. Trains locally when the server starts a round.
7. Writes updated weights back into the JSON configuration.
8. Reports the number of local training samples to the server.
9. Sends updated weights when requested.

---

## 3. Main components

### 3.1 C++ layer

The C++ layer handles:

- sockets and TCP communication;
- protocol messages;
- authentication handshake;
- client registration;
- server menu;
- dataset transfer;
- training-round coordination;
- FedAvg aggregation;
- optional backdoor defense;
- model-weight serialization;
- JSON loading and weight updates;
- logging.

Important paths:

```text
main_server.cpp
main_client.cpp
Makefile
code/headers/
code/sources/
```

The C++ layer does not train neural networks directly. Instead, it invokes Python scripts with commands such as:

```bash
PYTHONPATH=code python3 -m FedSolPython.scripts.train <configClient.json>
```

and:

```bash
PYTHONPATH=code python3 -m FedSolPython.scripts.evaluate <config.json>
```

Because of that, the executables should normally be run from the repository root.

### 3.2 Python layer

The Python layer handles:

- model construction from JSON architecture definitions;
- model loading from `.pt` files;
- extraction and injection of flattened weights;
- local training;
- YOLO training;
- dataset splitting;
- dataset unpacking;
- sample counting;
- model evaluation;
- report generation.

Important paths:

```text
code/FedSolPython/training/
code/FedSolPython/data/
code/FedSolPython/evaluation/
code/FedSolPython/scripts/
code/FedSolPython/scripts_yolo/
```

---

## 4. Supported operating modes

FedSol Secure has two independent configuration decisions:

1. **How the model is described.**
2. **What type of dataset and trainer is used.**

These two decisions are related, but they are not exactly the same.

---

## 5. Model-definition modes

### 5.1 JSON architecture mode

This mode is used when the `model` section does not contain `pt_path`.

Example:

```json
"model": {
  "id": 0,
  "architecture": {
    "layers": []
  },
  "weights": []
}
```

In this mode, the model architecture is declared manually in JSON using the FedSol layer DSL. The server and every client must declare the same architecture. During the normal handshake, the client sends its architecture to the server, and the server accepts the client only if the architecture matches the global architecture.

This mode is intended for classic PyTorch models that can be represented with the supported layer types:

| Code | Layer |
|---:|---|
| `1` | Input |
| `2` | Conv2d |
| `3` | MaxPool2d |
| `4` | Flatten |
| `5` | Dense / Linear |
| `6` | BatchNorm |
| `7` | Dropout |
| `8` | Activation |

This is the safest mode for small CNNs, MLPs, classification experiments, and regression experiments.

### 5.2 `pt_path` mode for generic PyTorch models

This mode is used when `model.pt_path` exists and `dataset.type` is not `yolo`.

Example:

```json
"dataset": {
  "type": "npz",
  "train_path": "input_client/dataset_train.npz",
  "test_path": "input_client/dataset_test.npz",
  "target_dtype": "long"
},
"model": {
  "id": 1,
  "pt_path": "input_client/model.pt"
}
```

Here, `pt_path` points to a saved PyTorch model. The system loads the `.pt` file and infers a FedSol-compatible architecture from its leaf modules. This mode still has an important limitation: non-YOLO generic `.pt` models must be expressible through the supported FedSol architecture types.

In other words, using `pt_path` does not remove the architecture limitations for classic models. It only avoids writing the architecture manually.

The Python inference scripts map these PyTorch modules:

| PyTorch module | FedSol layer type |
|---|---|
| `nn.Conv2d` | `2` / Conv2d |
| `nn.MaxPool2d` | `3` / MaxPool2d |
| `nn.Flatten` | `4` / Flatten |
| `nn.Linear` | `5` / Dense |
| `nn.BatchNorm2d`, `nn.BatchNorm1d` | `6` / BatchNorm |
| `nn.Dropout`, `nn.Dropout2d` | `7` / Dropout |
| `nn.ReLU`, `nn.Sigmoid`, `nn.Tanh`, `nn.Softmax` | `8` / Activation |
| `nn.Identity` | ignored |

Unsupported modules are warned about or ignored during inference. If the `.pt` model depends on unsupported operations, the inferred architecture may be incomplete, and weight compatibility can fail later.

### 5.3 `pt_path` mode for YOLO

This mode is used when both conditions are true:

```json
"dataset": {
  "type": "yolo"
}
```

and:

```json
"model": {
  "pt_path": "input_client/model.pt"
}
```

In this mode, the client does not use the classic `Trainer`. It uses `YOLOTrainer`. YOLO training is handled through Ultralytics, and the dataset is described as a YOLO dataset with `train`, `val`, `nc`, and `names`.

For YOLO mode, `model.pt_path` is required. If `dataset.type` is `yolo` and `model.pt_path` is missing, the YOLO trainer fails because it has no YOLO base model to load.

YOLO mode has its own behavior:

- the `.pt` file is loaded with Ultralytics `YOLO` when possible;
- the detection head can be rebuilt if `dataset.nc` differs from the base model class count;
- class names can be synchronized from `dataset.names`;
- weights include trainable parameters and BatchNorm running statistics;
- the dataset YAML is generated automatically during local training;
- local training uses `model.train(...)` from Ultralytics.

This mode is intended for object detection only.

---

## 6. Dataset modes

### 6.1 `.npz` datasets

Used for classification and regression tasks.

Expected structure:

```text
X -> input tensor array
y -> target array
```

Example:

```json
"dataset": {
  "type": "npz",
  "train_path": "input_server/dataset_train.npz",
  "test_path": "input_server/dataset_test.npz",
  "target_dtype": "long"
}
```

`target_dtype` can be:

- `long`, normally for classification targets;
- `float`, normally for regression targets.

### 6.2 YOLO datasets

Used for object detection tasks.

Expected server-side structure:

```text
input_server/dataset/
├── train/
│   ├── images/
│   └── labels/
└── val/
    ├── images/
    └── labels/
```

Example server dataset section:

```json
"dataset": {
  "type": "yolo",
  "train_path": "input_server/dataset",
  "val_path": "input_server/dataset/val/images",
  "nc": 1,
  "names": ["pina"]
}
```

Example client dataset section:

```json
"dataset": {
  "type": "yolo",
  "receive_path": "input_client/dataset.tar.gz",
  "train_path": "input_client/dataset_local/dataset/train",
  "val_path": "input_client/dataset_local/dataset/val",
  "nc": 1,
  "names": ["pina"]
}
```

The server can split a YOLO directory into one `.tar.gz` package per client. The client can unpack the received package automatically before training.

---

## 7. Authentication

The client sends a plaintext password from `configClient.json` during the handshake. The server hashes that received password with SHA-256 and compares the result with the value stored in `.env`.

The repository root must contain:

```env
SERVER_PASSWORD_HASH=<sha256_hash_of_the_client_password>
```

For example, if every client has:

```json
"password": "federated123"
```

then `.env` must contain the SHA-256 hash of `federated123`.

If the hash does not match, the server rejects the client.

---

## 8. Handshake compatibility checks

FedSol Secure uses two different compatibility checks depending on the model mode.

### 8.1 Normal JSON architecture mode

The client sends its architecture to the server. The server compares it against the global architecture.

Possible result:

```text
ARCH_OK
```

or:

```text
ARCH_INVALID
```

### 8.2 `pt_path` mode

The client and the server compute a stable architecture hash from the `.pt` model. The hash excludes the weights and focuses on the model structure.

Possible result:

```text
HASH_OK
```

or:

```text
HASH_INVALID
```

This is used both for generic `.pt` mode and YOLO `.pt` mode.

---

## 9. Server menu

After starting, the server exposes an interactive menu:

```text
1. Start Training
2. List Clients
3. Populate Random Weights
4. Evaluate Model
5. Send train data
6. Stop client(s)
7. Exit
```

### Option 1 — Start Training

Starts a federated training round. The server asks for the number of epochs at the federated level. It then sends the current global weights to each connected client. Each client trains locally using its own `training.epochs` value from `configClient.json`.

### Option 2 — List Clients

Prints registered clients and their states.

### Option 3 — Populate Random Weights

Available mainly for JSON architecture mode. It computes the expected number of weights from the declared architecture and writes randomly generated weights into the server config.

In `pt_path` mode, the server extracts weights from the `.pt` file, so this option is not usually needed.

### Option 4 — Evaluate Model

Runs the Python evaluator using the server configuration. The evaluator loads the latest exported model from:

```text
output_server/models/
```

and produces metrics and reports according to `evaluation`.

### Option 5 — Send train data

Splits the server training dataset according to the number of connected clients and sends one shard to each client.

For `.npz`, it creates files such as:

```text
input_server/dataset_train_0.npz
input_server/dataset_train_1.npz
```

For YOLO directories, it creates files such as:

```text
input_server/dataset_train_0.tar.gz
input_server/dataset_train_1.tar.gz
```

### Option 6 — Stop client(s)

Sends a shutdown message to one client or all clients.

### Option 7 — Exit

Terminates the server process.

---

## 10. Backdoor defense

The server configuration includes an optional top-level field:

```json
"defense": true
```

When enabled, the server applies a filter before FedAvg. The filter compares each client update against the previous global model:

```text
client_update = client_weights - previous_global_weights
```

It computes update norms, uses the median norm as a clipping threshold, and rescales updates whose norm exceeds that threshold.

Conceptual log:

```text
[NormClipping] clip_norm=9.610454
[NormClipping] Client 2 clipped: norm 13.322386 -> 9.610454
```

The purpose is to reduce the effect of abnormally large updates, which may occur in poisoned-client experiments or unstable local training.

---

## 11. Basic execution workflow

### 11.1 Compile

From the repository root:

```bash
make clean
make
```

This generates:

```text
server_app
client_app
```

### 11.2 Create `.env`

Create a `.env` file in the repository root:

```env
SERVER_PASSWORD_HASH=<sha256_hash>
```

### 11.3 Prepare server files

Typical server input directory:

```text
input_server/
├── dataset_train.npz
├── dataset_test.npz
└── model.pt
```

or, for YOLO:

```text
input_server/
├── dataset/
│   ├── train/
│   │   ├── images/
│   │   └── labels/
│   └── val/
│       ├── images/
│       └── labels/
└── model.pt
```

### 11.4 Prepare client files

Typical client input directory:

```text
input_client/
├── configClient.json
├── dataset_train.npz
├── dataset_test.npz
└── model.pt
```

or, for YOLO:

```text
input_client/
├── configClient.json
├── model.pt
└── dataset.tar.gz
```

### 11.5 Start the server

```bash
./server_app configTest/test3/config.json
```

### 11.6 Start each client

On each client node:

```bash
./client_app input_client/configClient.json
```

Each client must have a unique `client.id`.

### 11.7 Send data if needed

From the server menu, choose:

```text
5. Send train data
```

### 11.8 Start training

From the server menu, choose:

```text
1. Start Training
```

### 11.9 Evaluate

After at least one training round, choose:

```text
4. Evaluate Model
```

---

## 12. Output files

Common outputs:

```text
output_server/logs/server.log
output_server/models/model_<timestamp>.pt
output_server/globalResults/
output_client/logs/client_<id>.log
output_client/stats/<date>/run_<time>.json
output_client/yolo_runs/
```

The exported global model is saved under `output_server/models/`. In `pt_path` mode, the system can also write the updated model back to the configured `model.pt` path so that subsequent rounds start from the current global model.

---

## 13. Related documentation

This documentation set includes four final Markdown files:

```text
README.md
README_config_server.md
README_config_client.md
README_environment_requirements.md
```

Use them together:

- `README.md` explains the project as a whole.
- `README_config_server.md` explains the server configuration.
- `README_config_client.md` explains the client configuration.
- `README_environment_requirements.md` explains installation and dependencies.
