# FedSol Secure — Client `configClient.json` Guide

This document explains how to fill the client configuration file used by:

```bash
./client_app <path_to_configClient.json>
```

Example:

```bash
./client_app input_client/configClient.json
```

Each client configuration defines the client identity, the server connection, the local or received dataset, the local model source, the local training parameters, and optional evaluation metadata.

---

## 1. Minimal file structure

A complete client config normally follows this structure:

```json
{
  "client": {},
  "connection": {},
  "dataset": {},
  "evaluation": {},
  "model": {},
  "training": {}
}
```

The required sections depend on the mode.

FedSol Secure supports these client-side modes:

1. JSON architecture mode.
2. Generic PyTorch `pt_path` mode.
3. YOLO `pt_path` mode.

The difference between the two `pt_path` cases is essential:

```text
pt_path + dataset.type != "yolo"  -> classic PyTorch Trainer
pt_path + dataset.type == "yolo" -> YOLOTrainer
```

Therefore, `pt_path` by itself does not mean YOLO.

---

## 2. Client section

Example:

```json
"client": {
  "id": 3
}
```

### 2.1 `id`

Unique logical identifier for this client.

Examples:

```json
"id": 1
```

```json
"id": 2
```

```json
"id": 3
```

Rules:

- It must be an integer.
- It should be positive.
- It must be unique among all clients connected to the same server.

If two clients use the same ID, the server rejects the second one with an ID-duplicate response.

Practical setup:

```text
Client on Jetson 1 -> id = 1
Client on Jetson 2 -> id = 2
Client on laptop   -> id = 3
```

The ID is also used in logs such as:

```text
output_client/logs/client_3.log
```

---

## 3. Connection section

Example:

```json
"connection": {
  "password": "federated123",
  "port": 9000,
  "server_ip": "192.168.68.118"
}
```

This section tells the client how to reach and authenticate with the server.

### 3.1 `password`

```json
"password": "federated123"
```

Plaintext password sent by the client during handshake.

The server does not store this password directly. The server hashes the received password with SHA-256 and compares it with:

```env
SERVER_PASSWORD_HASH=<hash>
```

inside the server-side `.env` file.

If the password is wrong, the server rejects the client.

### 3.2 `port`

```json
"port": 9000
```

The TCP port where the server is listening.

This value must match the server `config.json`:

```json
"port": 9000
```

### 3.3 `server_ip`

```json
"server_ip": "192.168.68.118"
```

IP address of the machine running `server_app`.

Examples:

```json
"server_ip": "127.0.0.1"
```

when client and server run on the same machine.

```json
"server_ip": "192.168.68.118"
```

when the server runs on another computer in the local network.

---

## 4. Dataset section

The `dataset` section tells the client where local data is stored and which training mode to use.

FedSol Secure supports:

1. `npz`, for classic classification or regression.
2. `yolo`, for object detection with Ultralytics YOLO.

---

## 5. Client dataset in `.npz` mode

Example:

```json
"dataset": {
  "type": "npz",
  "train_path": "input_client/dataset_train.npz",
  "test_path": "input_client/dataset_test.npz",
  "target_dtype": "long"
}
```

### 5.1 `type`

```json
"type": "npz"
```

This selects the classic PyTorch `Trainer`, unless a different non-YOLO type is implemented later.

The `.npz` file must contain:

```text
X
y
```

### 5.2 `train_path`

```json
"train_path": "input_client/dataset_train.npz"
```

Path to the local training dataset used by this client.

During local training, the Python `DatasetFactory` reads this file and creates a PyTorch `DataLoader`.

### 5.3 `test_path`

```json
"test_path": "input_client/dataset_test.npz"
```

Optional or evaluation-oriented test dataset path. It is useful when the same client config is reused for local evaluation or diagnostics.

### 5.4 `target_dtype`

```json
"target_dtype": "long"
```

Valid values:

| Value | Meaning | Typical use |
|---|---|---|
| `long` | Convert targets with `.long()` | classification |
| `float` | Convert targets with `.float()` | regression or continuous labels |

For multiclass classification with `cross_entropy`, use:

```json
"target_dtype": "long"
```

For regression with `mse` or `mae`, use:

```json
"target_dtype": "float"
```

---

## 6. Client dataset in YOLO mode

Example:

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

### 6.1 `type`

```json
"type": "yolo"
```

This selects the YOLO training path.

Important rule:

```text
If dataset.type is yolo, the model section must contain pt_path.
```

The client uses `YOLOTrainer`, which requires a `.pt` model file.

### 6.2 `receive_path`

```json
"receive_path": "input_client/dataset.tar.gz"
```

Path where the client receives a dataset package sent by the server.

The C++ code chooses the dataset path using this logic:

```text
if dataset.receive_path exists in config -> use receive_path
else -> use dataset.train_path
```

For YOLO distributed experiments, `receive_path` is normally used because the server sends a `.tar.gz` shard to each client.

When training starts, the Python code checks whether this archive exists. If it exists, it unpacks it under:

```text
input_client/dataset_local/
```

and updates the config paths automatically.

### 6.3 `train_path`

```json
"train_path": "input_client/dataset_local/dataset/train"
```

Path to the client training directory after unpacking.

Expected layout:

```text
train/
├── images/
└── labels/
```

The YOLO trainer generates a temporary YAML file from this path.

### 6.4 `val_path`

```json
"val_path": "input_client/dataset_local/dataset/val"
```

Path to the validation directory after unpacking.

Expected layout:

```text
val/
├── images/
└── labels/
```

If no validation directory exists, the unpacking logic can fall back to the training directory.

### 6.5 `nc`

```json
"nc": 1
```

Number of object classes.

This value must match the server value. It is used to rebuild the YOLO detection head when necessary.

### 6.6 `names`

```json
"names": ["pina"]
```

List of class names.

Rules:

- Length should equal `nc`.
- The order must match the YOLO label class IDs.

For one class:

```json
"nc": 1,
"names": ["pina"]
```

For two classes:

```json
"nc": 2,
"names": ["healthy", "poisoned"]
```

---

## 7. Model section

The `model` section defines how the client obtains its model.

Supported modes:

1. JSON architecture mode.
2. Generic PyTorch `pt_path` mode.
3. YOLO `pt_path` mode.

---

## 8. JSON architecture mode

This mode is selected when `model.pt_path` is absent.

Example:

```json
"model": {
  "id": 1,
  "architecture": {
    "layers": []
  },
  "weights": []
}
```

In this mode, the client sends its architecture to the server during handshake. The server accepts the client only if the architecture matches the server architecture.

### 8.1 `id`

```json
"id": 1
```

Logical model ID. In practice, the client identity comes from `client.id`, while this field identifies the model object in the JSON.

### 8.2 `architecture.layers`

Manual list of layers in FedSol format.

Supported layer types:

| Code | Layer | Meaning |
|---:|---|---|
| `1` | Input | input metadata |
| `2` | Conv2d | convolution |
| `3` | MaxPool2d | max pooling |
| `4` | Flatten | tensor flattening |
| `5` | Dense | fully connected / linear layer |
| `6` | BatchNorm | batch normalization |
| `7` | Dropout | dropout regularization |
| `8` | Activation | activation layer |

Activation codes:

| Code | Activation |
|---:|---|
| `0` | Identity / none |
| `1` | Identity / none |
| `2` | ReLU |
| `3` | Sigmoid |
| `4` | Tanh |
| `5` | Softmax |

### 8.3 `weights`

```json
"weights": []
```

Flat weight vector.

On the client, this value is overwritten during the federated workflow:

1. The server sends global weights.
2. The client writes them into `configClient.json`.
3. Python trains locally.
4. Python writes updated local weights back into `configClient.json`.
5. The C++ client sends those updated weights back to the server.

In many initial client configs, `weights` can start empty or as a placeholder, because the server sends the actual global weights at the beginning of a round.

---

## 9. How to define layers manually

Each layer uses a common object structure:

```json
{
  "type": 0,
  "input_dim": [0, 0, 0],
  "output_dim": [0, 0, 0],
  "kernel_size": 0,
  "stride": 0,
  "padding": 0,
  "in_features": 0,
  "out_features": 0,
  "activation": 0
}
```

### 9.1 `input_dim` and `output_dim`

Use:

```text
[height, width, channels]
```

Example for a grayscale `28 x 28` image:

```json
"input_dim": [28, 28, 1]
```

Example for 8 output feature maps:

```json
"output_dim": [28, 28, 8]
```

### 9.2 `kernel_size`, `stride`, and `padding`

Used mainly by convolution and pooling layers.

Example convolution:

```json
"kernel_size": 3,
"stride": 1,
"padding": 1
```

### 9.3 `in_features` and `out_features`

Used mainly by dense layers and flatten metadata.

Example:

```json
"in_features": 784,
"out_features": 128
```

### 9.4 `activation`

Only meaningful when:

```json
"type": 8
```

For ReLU:

```json
"activation": 2
```

---

## 10. Example layer sequence for Fashion-MNIST

```text
Input -> Conv2d -> ReLU -> MaxPool2d -> Flatten -> Dense
```

JSON example:

```json
"architecture": {
  "layers": [
    {
      "type": 1,
      "input_dim": [28, 28, 1],
      "output_dim": [28, 28, 1],
      "kernel_size": 0,
      "stride": 0,
      "padding": 0,
      "in_features": 0,
      "out_features": 0,
      "activation": 0
    },
    {
      "type": 2,
      "input_dim": [28, 28, 1],
      "output_dim": [28, 28, 8],
      "kernel_size": 3,
      "stride": 1,
      "padding": 1,
      "in_features": 0,
      "out_features": 8,
      "activation": 0
    },
    {
      "type": 8,
      "input_dim": [28, 28, 8],
      "output_dim": [28, 28, 8],
      "kernel_size": 0,
      "stride": 0,
      "padding": 0,
      "in_features": 0,
      "out_features": 0,
      "activation": 2
    },
    {
      "type": 3,
      "input_dim": [28, 28, 8],
      "output_dim": [14, 14, 8],
      "kernel_size": 2,
      "stride": 2,
      "padding": 0,
      "in_features": 0,
      "out_features": 0,
      "activation": 0
    },
    {
      "type": 4,
      "input_dim": [14, 14, 8],
      "output_dim": [1, 1, 1],
      "kernel_size": 0,
      "stride": 0,
      "padding": 0,
      "in_features": 1568,
      "out_features": 1568,
      "activation": 0
    },
    {
      "type": 5,
      "input_dim": [1, 1, 1],
      "output_dim": [1, 1, 1],
      "kernel_size": 0,
      "stride": 0,
      "padding": 0,
      "in_features": 1568,
      "out_features": 10,
      "activation": 0
    }
  ]
}
```

Flatten calculation:

```text
14 * 14 * 8 = 1568
```

The final dense layer has `out_features = 10` because Fashion-MNIST has 10 classes.

---

## 11. Generic PyTorch `pt_path` mode

This mode is selected when `model.pt_path` exists and `dataset.type` is not `yolo`.

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

This means:

```text
Use a .pt model, but train it with the classic Trainer.
```

It does not mean YOLO.

The client startup logic does the following:

1. Detects `model.pt_path`.
2. Runs `load_pt_arch` to infer architecture from the `.pt` file.
3. Computes a stable `.pt` architecture hash.
4. Reloads the updated JSON.
5. Starts the client in hash-handshake mode.

During local training, the classic Python `Trainer`:

1. Loads the `.pt` model.
2. Applies server weights to the model.
3. Trains using the `.npz` dataset.
4. Extracts model parameters.
5. Writes the updated flat weights back to `configClient.json`.

Important limitation:

```text
Generic pt_path still requires the model to be compatible with supported FedSol layer types.
```

Supported PyTorch modules for architecture inference:

| PyTorch module | FedSol type |
|---|---|
| `nn.Conv2d` | Conv2d |
| `nn.MaxPool2d` | MaxPool2d |
| `nn.Flatten` | Flatten |
| `nn.Linear` | Dense |
| `nn.BatchNorm2d`, `nn.BatchNorm1d` | BatchNorm |
| `nn.Dropout`, `nn.Dropout2d` | Dropout |
| `nn.ReLU`, `nn.Sigmoid`, `nn.Tanh`, `nn.Softmax` | Activation |
| `nn.Identity` | ignored |

Avoid generic `.pt` models that depend on operations not represented by those modules.

---

## 12. YOLO `pt_path` mode

This mode is selected when:

```json
"dataset": {
  "type": "yolo"
}
```

and:

```json
"model": {
  "id": 4,
  "pt_path": "input_client/model.pt"
}
```

Complete example:

```json
{
  "client": {
    "id": 3
  },
  "connection": {
    "password": "federated123",
    "port": 9000,
    "server_ip": "192.168.68.118"
  },
  "dataset": {
    "type": "yolo",
    "receive_path": "input_client/dataset.tar.gz",
    "train_path": "input_client/dataset_local/dataset/train",
    "val_path": "input_client/dataset_local/dataset/val",
    "nc": 1,
    "names": ["pina"]
  },
  "model": {
    "id": 4,
    "pt_path": "input_client/model.pt"
  },
  "training": {
    "epochs": 2,
    "batch_size": 2,
    "imgsz": 320,
    "lr": 0.01,
    "optimizer": "sgd",
    "loss": {
      "name": "yolo_default",
      "params": {}
    }
  },
  "evaluation": {
    "task_type": "object_detection",
    "prediction_config": {
      "conf": 0.25,
      "iou": 0.45
    },
    "metrics": [
      "map50",
      "map50_95",
      "precision",
      "recall"
    ],
    "report_function": {
      "name": "default_report",
      "params": {
        "base_dir": "globalResults",
        "print_to_console": true,
        "save_results": true,
        "use_timestamp_path": true
      }
    }
  }
}
```

In YOLO mode, the client uses `YOLOTrainer`, not the classic `Trainer`.

The YOLO trainer:

1. Unpacks the received dataset if `receive_path` exists.
2. Loads the YOLO `.pt` model.
3. Rebuilds the detection head if `dataset.nc` requires it.
4. Injects server weights into the YOLO model.
5. Generates a `dataset_yolo.yaml` file.
6. Runs Ultralytics training.
7. Reloads the best model if available.
8. Extracts weights and BatchNorm running statistics.
9. Writes updated weights to `configClient.json`.

---

## 13. Training section

The `training` section controls local training performed by each client.

---

## 14. Training for `.npz` clients

Example:

```json
"training": {
  "epochs": 5,
  "batch_size": 32,
  "lr": 0.001,
  "optimizer": "adam",
  "loss": {
    "name": "cross_entropy",
    "params": {}
  }
}
```

### 14.1 `epochs`

Number of local epochs performed by this client during each server round.

This is different from the number of global rounds entered in the server menu.

Example:

```text
Server menu asks for 3 epochs/rounds.
Client config has training.epochs = 2.
```

Conceptually, the server coordinates 3 federated training cycles, and in each cycle the client trains locally for 2 epochs.

### 14.2 `batch_size`

Batch size used by the client `DataLoader`.

Example:

```json
"batch_size": 32
```

On memory-limited devices, reduce it:

```json
"batch_size": 2
```

### 14.3 `lr`

Learning rate.

Example:

```json
"lr": 0.001
```

### 14.4 `optimizer`

Supported values:

```json
"optimizer": "adam"
```

or:

```json
"optimizer": "sgd"
```

### 14.5 `loss`

Supported loss values:

| Name | PyTorch loss |
|---|---|
| `cross_entropy` | `nn.CrossEntropyLoss` |
| `mse` | `nn.MSELoss` |
| `mae` | `nn.L1Loss` |
| `bce` | `nn.BCELoss` |
| `bce_logits` | `nn.BCEWithLogitsLoss` |
| `bce_with_logits` | `nn.BCEWithLogitsLoss` |

Example:

```json
"loss": {
  "name": "cross_entropy",
  "params": {}
}
```

---

## 15. Training for YOLO clients

Example:

```json
"training": {
  "epochs": 10,
  "batch_size": 2,
  "imgsz": 320,
  "lr": 0.01,
  "optimizer": "sgd",
  "loss": {
    "name": "yolo_default",
    "params": {}
  }
}
```

### 15.1 `epochs`

Number of local YOLO epochs per federated round.

### 15.2 `batch_size`

YOLO training batch size.

For Jetson Nano or small devices, values such as `1` or `2` are more realistic.

### 15.3 `imgsz`

Image size passed to YOLO training.

Examples:

```json
"imgsz": 320
```

or:

```json
"imgsz": 640
```

Smaller values reduce memory and training time.

### 15.4 `lr`

Learning rate passed to YOLO training.

### 15.5 `optimizer`

Stored for consistency. The YOLO training call is handled by Ultralytics.

### 15.6 `loss`

Use:

```json
"loss": {
  "name": "yolo_default",
  "params": {}
}
```

This is informational because YOLO uses its own detection loss internally.

---

## 16. Evaluation section

The client configuration may include evaluation metadata. In the federated workflow, server-side evaluation is the most important path, but keeping this section in client configs helps maintain consistency.

Classification example:

```json
"evaluation": {
  "task_type": "multiclass_classification",
  "prediction_config": {
    "method": "argmax"
  },
  "metrics": [
    { "name": "loss" },
    { "name": "accuracy" },
    { "name": "precision", "average": "macro", "zero_division": 0 },
    { "name": "recall", "average": "macro", "zero_division": 0 },
    { "name": "f1", "average": "macro", "zero_division": 0 },
    { "name": "confusion_matrix" }
  ],
  "report_function": {
    "name": "default_report",
    "params": {
      "base_dir": "globalResults",
      "print_to_console": true,
      "save_results": true,
      "use_timestamp_path": true
    }
  }
}
```

YOLO example:

```json
"evaluation": {
  "task_type": "object_detection",
  "prediction_config": {
    "conf": 0.25,
    "iou": 0.45
  },
  "metrics": [
    "map50",
    "map50_95",
    "precision",
    "recall"
  ],
  "report_function": {
    "name": "default_report",
    "params": {
      "base_dir": "globalResults",
      "print_to_console": true,
      "save_results": true,
      "use_timestamp_path": true
    }
  }
}
```

---

## 17. Complete example — `.npz` with manual JSON architecture

```json
{
  "client": {
    "id": 1
  },
  "connection": {
    "password": "federated123",
    "port": 9000,
    "server_ip": "192.168.68.118"
  },
  "dataset": {
    "type": "npz",
    "train_path": "input_client/dataset_train.npz",
    "test_path": "input_client/dataset_test.npz",
    "target_dtype": "long"
  },
  "model": {
    "id": 1,
    "architecture": {
      "layers": [
        {
          "type": 1,
          "input_dim": [28, 28, 1],
          "output_dim": [28, 28, 1],
          "kernel_size": 0,
          "stride": 0,
          "padding": 0,
          "in_features": 0,
          "out_features": 0,
          "activation": 0
        },
        {
          "type": 2,
          "input_dim": [28, 28, 1],
          "output_dim": [28, 28, 8],
          "kernel_size": 3,
          "stride": 1,
          "padding": 1,
          "in_features": 0,
          "out_features": 8,
          "activation": 0
        },
        {
          "type": 8,
          "input_dim": [28, 28, 8],
          "output_dim": [28, 28, 8],
          "kernel_size": 0,
          "stride": 0,
          "padding": 0,
          "in_features": 0,
          "out_features": 0,
          "activation": 2
        },
        {
          "type": 3,
          "input_dim": [28, 28, 8],
          "output_dim": [14, 14, 8],
          "kernel_size": 2,
          "stride": 2,
          "padding": 0,
          "in_features": 0,
          "out_features": 0,
          "activation": 0
        },
        {
          "type": 4,
          "input_dim": [14, 14, 8],
          "output_dim": [1, 1, 1],
          "kernel_size": 0,
          "stride": 0,
          "padding": 0,
          "in_features": 1568,
          "out_features": 1568,
          "activation": 0
        },
        {
          "type": 5,
          "input_dim": [1, 1, 1],
          "output_dim": [1, 1, 1],
          "kernel_size": 0,
          "stride": 0,
          "padding": 0,
          "in_features": 1568,
          "out_features": 10,
          "activation": 0
        }
      ]
    },
    "weights": []
  },
  "training": {
    "epochs": 5,
    "batch_size": 32,
    "lr": 0.001,
    "optimizer": "adam",
    "loss": {
      "name": "cross_entropy",
      "params": {}
    }
  },
  "evaluation": {
    "task_type": "multiclass_classification",
    "prediction_config": {
      "method": "argmax"
    },
    "metrics": [
      { "name": "loss" },
      { "name": "accuracy" }
    ],
    "report_function": {
      "name": "default_report",
      "params": {
        "base_dir": "globalResults",
        "print_to_console": true,
        "save_results": true,
        "use_timestamp_path": true
      }
    }
  }
}
```

---

## 18. Complete example — `.npz` with generic PyTorch `pt_path`

```json
{
  "client": {
    "id": 1
  },
  "connection": {
    "password": "federated123",
    "port": 9000,
    "server_ip": "192.168.68.118"
  },
  "dataset": {
    "type": "npz",
    "train_path": "input_client/dataset_train.npz",
    "test_path": "input_client/dataset_test.npz",
    "target_dtype": "long"
  },
  "model": {
    "id": 1,
    "pt_path": "input_client/model.pt"
  },
  "training": {
    "epochs": 5,
    "batch_size": 32,
    "lr": 0.001,
    "optimizer": "adam",
    "loss": {
      "name": "cross_entropy",
      "params": {}
    }
  },
  "evaluation": {
    "task_type": "multiclass_classification",
    "prediction_config": {
      "method": "argmax"
    },
    "metrics": [
      { "name": "loss" },
      { "name": "accuracy" }
    ],
    "report_function": {
      "name": "default_report",
      "params": {
        "base_dir": "globalResults",
        "print_to_console": true,
        "save_results": true,
        "use_timestamp_path": true
      }
    }
  }
}
```

This example uses `pt_path`, but it is not YOLO because `dataset.type` is `npz`.

---

## 19. Complete example — YOLO `pt_path`

```json
{
  "client": {
    "id": 3
  },
  "connection": {
    "password": "federated123",
    "port": 9000,
    "server_ip": "192.168.68.118"
  },
  "dataset": {
    "type": "yolo",
    "receive_path": "input_client/dataset.tar.gz",
    "train_path": "input_client/dataset_local/dataset/train",
    "val_path": "input_client/dataset_local/dataset/val",
    "nc": 1,
    "names": ["pina"]
  },
  "model": {
    "id": 4,
    "pt_path": "input_client/model.pt"
  },
  "training": {
    "epochs": 2,
    "batch_size": 2,
    "imgsz": 320,
    "lr": 0.01,
    "optimizer": "sgd",
    "loss": {
      "name": "yolo_default",
      "params": {}
    }
  },
  "evaluation": {
    "task_type": "object_detection",
    "prediction_config": {
      "conf": 0.25,
      "iou": 0.45
    },
    "metrics": [
      "map50",
      "map50_95",
      "precision",
      "recall"
    ],
    "report_function": {
      "name": "default_report",
      "params": {
        "base_dir": "globalResults",
        "print_to_console": true,
        "save_results": true,
        "use_timestamp_path": true
      }
    }
  }
}
```

---

## 20. Practical checklist

Before running a client:

1. Make sure `client.id` is unique.
2. Make sure `connection.server_ip` points to the server machine.
3. Make sure `connection.port` matches the server port.
4. Make sure `connection.password` corresponds to the server `.env` hash.
5. For `.npz`, make sure `train_path` exists and contains `X` and `y`.
6. For YOLO, make sure `pt_path` exists.
7. For YOLO, make sure `nc` and `names` match the server.
8. For YOLO, keep `receive_path` if the server will send dataset shards.
9. For generic `.pt`, make sure the model uses supported layer types.
10. For manual JSON architecture mode, make sure the architecture matches the server exactly.
