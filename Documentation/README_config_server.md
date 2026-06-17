# FedSol Secure — Server `config.json` Guide

This document explains how to fill the server configuration file used by:

```bash
./server_app <path_to_config.json>
```

Example:

```bash
./server_app configTest/test3/config.json
```

The server configuration defines the central experiment setup: network port, socket backlog, optional backdoor defense, global dataset paths, global model source, evaluation settings, and training parameters used by the Python layer.

---

## 1. Minimal file structure

A complete server config normally follows this structure:

```json
{
  "port": 9000,
  "backlog": 10,
  "defense": false,
  "dataset": {},
  "evaluation": {},
  "model": {},
  "training": {}
}
```

Some fields are read directly by C++ and some are consumed by Python scripts.

| Section | Used mainly by | Purpose |
|---|---|---|
| `port` | C++ | TCP port where the server listens |
| `backlog` | C++ | Pending socket connection queue size |
| `defense` | C++ | Enables or disables the backdoor-defense filter |
| `dataset` | C++ and Python | Dataset splitting, training, sample counting, evaluation |
| `model` | C++ and Python | Global model architecture, weights, or `.pt` path |
| `training` | Python | Loss, optimizer, local hyperparameters, YOLO settings |
| `evaluation` | Python | Task type, metrics, prediction logic, report generation |

---

## 2. Top-level server fields

### 2.1 `port`

Example:

```json
"port": 9000
```

This is the TCP port where the server listens for client connections.

Every client must use the same value in:

```json
"connection": {
  "port": 9000
}
```

Rules:

- Use an integer value.
- Make sure the port is not already used by another process.
- Make sure firewalls allow the port if clients run on different machines.

Common error:

```text
Client cannot connect because server uses port 9000 and client uses port 9100.
```

### 2.2 `backlog`

Example:

```json
"backlog": 10
```

This value controls the operating-system socket backlog. It is the number of pending TCP connections that the OS can queue before the server accepts them.

Recommended value for small experiments:

```json
"backlog": 10
```

This is not exactly the number of allowed clients. It is the pending connection queue for the listening socket.

### 2.3 `defense`

Example:

```json
"defense": false
```

or:

```json
"defense": true
```

This field enables or disables the backdoor-defense filter in the server aggregation pipeline.

If omitted, the code defaults to:

```text
false
```

When `defense` is `false`, the server aggregates valid client weights directly with FedAvg.

When `defense` is `true`, the server applies norm clipping to client updates before aggregation. The update is computed as:

```text
client_update = client_weights - previous_global_weights
```

The system computes the norm of each client update, takes the median update norm as clipping threshold, and scales down updates whose norm is larger than the threshold.

Example log:

```text
[NormClipping] clip_norm=9.610454
[NormClipping] Client 2 clipped: norm 13.322386 -> 9.610454
```

Use this option when running poisoned-client or backdoor-defense experiments.

---

## 3. Dataset section

The `dataset` section describes the server-side dataset. Its structure depends on the selected dataset mode.

FedSol Secure currently supports:

1. `npz`, for classic tensor datasets.
2. `yolo`, for object detection datasets.

---

## 4. Server dataset in `.npz` mode

Example:

```json
"dataset": {
  "type": "npz",
  "train_path": "input_server/dataset_train.npz",
  "test_path": "input_server/dataset_test.npz",
  "target_dtype": "long"
}
```

### 4.1 `type`

```json
"type": "npz"
```

This tells the Python layer to load the dataset with NumPy.

The `.npz` file must contain these keys:

```text
X
y
```

For example, for Fashion-MNIST:

```text
X -> shape (N, 1, 28, 28)
y -> shape (N,)
```

### 4.2 `train_path`

```json
"train_path": "input_server/dataset_train.npz"
```

This is the server-side training dataset.

The server uses this path when menu option 5 is selected:

```text
5. Send train data
```

For `.npz`, the split script creates one file per client:

```text
input_server/dataset_train_0.npz
input_server/dataset_train_1.npz
input_server/dataset_train_2.npz
```

### 4.3 `test_path`

```json
"test_path": "input_server/dataset_test.npz"
```

This is the dataset used for server-side evaluation.

It is read by the Python evaluator when the server menu option 4 is selected:

```text
4. Evaluate Model
```

### 4.4 `target_dtype`

```json
"target_dtype": "long"
```

Controls how targets are converted to PyTorch tensors.

Valid values:

| Value | PyTorch target dtype | Typical use |
|---|---|---|
| `long` | `torch.long` | classification with `cross_entropy` |
| `float` | `torch.float` | regression or binary targets |

Use `long` for multiclass classification labels.

Use `float` for regression targets or losses that expect floating-point labels.

---

## 5. Server dataset in YOLO mode

Example:

```json
"dataset": {
  "type": "yolo",
  "train_path": "input_server/dataset",
  "val_path": "input_server/dataset/val/images",
  "nc": 1,
  "names": ["pina"]
}
```

### 5.1 `type`

```json
"type": "yolo"
```

This selects YOLO object detection mode. In this mode, the Python training path uses `YOLOTrainer` on clients and YOLO-specific evaluation on the server.

Important rule:

```text
If dataset.type is yolo, model.pt_path is required.
```

YOLO mode is not compatible with manual JSON-only architecture mode because the YOLO trainer requires a `.pt` model.

### 5.2 `train_path`

```json
"train_path": "input_server/dataset"
```

This must point to the root directory of the YOLO dataset on the server.

Expected layout:

```text
input_server/dataset/
├── train/
│   ├── images/
│   └── labels/
└── val/
    ├── images/
    └── labels/
```

When menu option 5 is selected, the server split script reads:

```text
<train_path>/train/images
<train_path>/train/labels
```

and creates one `.tar.gz` package per client.

### 5.3 `val_path`

```json
"val_path": "input_server/dataset/val/images"
```

This is the validation path used by server-side object detection evaluation.

Depending on the evaluator path, this can point to the validation image directory. The YOLO trainer on clients generates its own YAML using the client `train_path` and `val_path`.

### 5.4 `nc`

```json
"nc": 1
```

Number of object classes.

This value is critical in YOLO mode. The code can rebuild the YOLO detection head if the base model was trained for a different number of classes.

Example:

```json
"nc": 1
```

means there is one class.

If the original `.pt` model has 80 classes and `nc` is set to 1, the system rebuilds the detection head to output one class.

### 5.5 `names`

```json
"names": ["pina"]
```

List of class names. The length should match `nc`.

For one class:

```json
"nc": 1,
"names": ["pina"]
```

For three classes:

```json
"nc": 3,
"names": ["class_a", "class_b", "class_c"]
```

---

## 6. Model section

The `model` section supports two main modes:

1. JSON architecture mode.
2. `pt_path` mode.

`pt_path` mode has two subcases:

1. Generic PyTorch `.pt` model with non-YOLO dataset.
2. YOLO `.pt` model with `dataset.type = "yolo"`.

---

## 7. Model in JSON architecture mode

This mode is selected when `model.pt_path` is absent.

Example:

```json
"model": {
  "id": 0,
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
      }
    ]
  },
  "weights": []
}
```

### 7.1 `id`

```json
"id": 0
```

Logical identifier for the global model.

### 7.2 `architecture.layers`

Manual layer list. The server builds an internal `Architecture` object from this list and verifies that clients have the same architecture.

Supported layer types:

| Code | Meaning | PyTorch equivalent |
|---:|---|---|
| `1` | Input | metadata only |
| `2` | Conv2d | `nn.Conv2d` |
| `3` | MaxPool2d | `nn.MaxPool2d` |
| `4` | Flatten | `nn.Flatten` |
| `5` | Dense | `nn.Linear` |
| `6` | BatchNorm | `nn.BatchNorm2d` or `nn.BatchNorm1d` in inference scripts |
| `7` | Dropout | `nn.Dropout` |
| `8` | Activation | `nn.ReLU`, `nn.Sigmoid`, `nn.Tanh`, `nn.Softmax` |

Activation codes:

| Code | Meaning |
|---:|---|
| `0` | no activation / identity |
| `1` | no activation / identity |
| `2` | ReLU |
| `3` | Sigmoid |
| `4` | Tanh |
| `5` | Softmax |

### 7.3 `weights`

```json
"weights": []
```

Flat vector of model weights.

In JSON architecture mode, before starting training, the server validates that the number of weights matches the expected count from `architecture.layers`.

If the vector is empty or invalid, menu option 1 refuses to start and suggests menu option 3:

```text
3. Populate Random Weights
```

Option 3 computes the expected number of weights and writes random values into `model.weights`.

---

## 8. Model in generic `pt_path` mode

This mode is selected when `model.pt_path` exists and the dataset is not YOLO.

Example:

```json
"dataset": {
  "type": "npz",
  "train_path": "input_server/dataset_train.npz",
  "test_path": "input_server/dataset_test.npz",
  "target_dtype": "long"
},
"model": {
  "id": 0,
  "pt_path": "input_server/model.pt"
}
```

This mode loads the model from a PyTorch `.pt` file.

Important clarification:

```text
pt_path does not automatically mean YOLO.
```

The code decides the trainer from `dataset.type`. If `dataset.type` is `npz`, the system uses the classic `Trainer`, even if `model.pt_path` exists.

In generic `pt_path` mode:

1. The server loads the `.pt` file.
2. The server infers a FedSol architecture from the PyTorch modules.
3. The server extracts model weights into `model.weights`.
4. The server computes an architecture hash from the `.pt` structure.
5. During handshake, clients are validated by hash instead of by serialized JSON architecture.

However, the model still needs to be compatible with the supported architecture set. The inference scripts only map these modules:

```text
Conv2d, MaxPool2d, Flatten, Linear, BatchNorm1d/2d, Dropout/Dropout2d, ReLU, Sigmoid, Tanh, Softmax, Identity
```

If the `.pt` file contains unsupported custom layers, attention blocks, residual logic, reshape operations, or other operations not represented by the FedSol layer DSL, the inferred architecture can be incomplete.

Use generic `pt_path` mode when:

- the model is saved as a full PyTorch `nn.Module`;
- the model can be represented by the supported layer types;
- the dataset is `.npz`;
- the task is classification or regression.

Do not use generic `pt_path` mode for YOLO object detection. Use YOLO `pt_path` mode instead.

---

## 9. Model in YOLO `pt_path` mode

This mode is selected when:

```json
"dataset": {
  "type": "yolo"
}
```

and:

```json
"model": {
  "id": 0,
  "pt_path": "input_server/model.pt"
}
```

Example:

```json
"dataset": {
  "type": "yolo",
  "train_path": "input_server/dataset",
  "val_path": "input_server/dataset/val/images",
  "nc": 1,
  "names": ["pina"]
},
"model": {
  "id": 0,
  "pt_path": "input_server/model.pt"
}
```

In YOLO mode:

- the `.pt` file is loaded as an Ultralytics YOLO model when possible;
- the server extracts trainable weights and BatchNorm running statistics;
- the detection head is rebuilt when `dataset.nc` differs from the base model class count;
- class names are synchronized from `dataset.names` when the global model is exported;
- the model is validated with a `.pt` architecture hash during client handshake.

Use YOLO `pt_path` mode when:

- `dataset.type` is `yolo`;
- the task is `object_detection`;
- the model is an Ultralytics-compatible `.pt` file;
- the dataset has YOLO image/label structure.

---

## 10. Training section

The meaning of `training` depends on the dataset mode.

---

## 11. Training for `.npz` tasks

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

### 11.1 `epochs`

Local epochs used by each client during local training.

### 11.2 `batch_size`

Mini-batch size used by the PyTorch `DataLoader`.

### 11.3 `lr`

Learning rate.

### 11.4 `optimizer`

Supported values:

```json
"optimizer": "adam"
```

or:

```json
"optimizer": "sgd"
```

### 11.5 `loss`

Supported loss names:

| Name | PyTorch loss |
|---|---|
| `cross_entropy` | `nn.CrossEntropyLoss` |
| `mse` | `nn.MSELoss` |
| `mae` | `nn.L1Loss` |
| `bce` | `nn.BCELoss` |
| `bce_logits` | `nn.BCEWithLogitsLoss` |
| `bce_with_logits` | `nn.BCEWithLogitsLoss` |
| `yolo_default` | Informational for YOLO; YOLO handles its own loss |

Example with class weights would follow PyTorch parameters if supported by the runtime object:

```json
"loss": {
  "name": "cross_entropy",
  "params": {}
}
```

---

## 12. Training for YOLO tasks

Example:

```json
"training": {
  "epochs": 3,
  "batch_size": 8,
  "imgsz": 640,
  "lr": 0.01,
  "optimizer": "sgd",
  "loss": {
    "name": "yolo_default",
    "params": {}
  }
}
```

### 12.1 `epochs`

Number of local YOLO epochs per client round.

### 12.2 `batch_size`

Batch size passed to Ultralytics training.

### 12.3 `imgsz`

YOLO image size.

Example:

```json
"imgsz": 320
```

for small devices, or:

```json
"imgsz": 640
```

for larger training runs.

### 12.4 `lr`

Learning rate passed as `lr0` to YOLO training.

### 12.5 `optimizer`

Stored for consistency. In the current YOLO training code, the training path explicitly calls Ultralytics `model.train(...)` with several parameters. Keep this field as documentation and consistency with the rest of the config.

### 12.6 `loss`

Use:

```json
"loss": {
  "name": "yolo_default",
  "params": {}
}
```

YOLO computes its own internal detection loss.

---

## 13. Evaluation section

The `evaluation` section controls how the server evaluates the exported model.

---

## 14. Evaluation for multiclass classification

Example:

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
      "base_dir": "output_server/globalResults",
      "print_to_console": true,
      "save_results": true,
      "use_timestamp_path": true
    }
  }
}
```

Supported task types and metrics:

| Task type | Metrics |
|---|---|
| `binary_classification` | `loss`, `accuracy`, `precision`, `recall`, `f1`, `confusion_matrix` |
| `multiclass_classification` | `loss`, `accuracy`, `precision`, `recall`, `f1`, `confusion_matrix` |
| `regression` | `loss`, `mse`, `mae`, `rmse` |
| `multivariate_regression` | `loss`, `mse`, `mae`, `rmse` |
| `object_detection` | `map50`, `map50_95`, `precision`, `recall` |

Prediction methods:

| Task type | Required or supported method |
|---|---|
| `multiclass_classification` | `argmax` |
| `binary_classification` | `argmax` or `threshold` |
| `regression` | `direct` |
| `multivariate_regression` | `direct` |

---

## 15. Evaluation for YOLO object detection

Example:

```json
"evaluation": {
  "task_type": "object_detection",
  "prediction_config": {
    "conf": 0.2,
    "iou": 0.4
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
      "base_dir": "output_server/globalResults",
      "print_to_console": true,
      "save_results": true,
      "use_timestamp_path": true
    }
  }
}
```

### 15.1 `conf`

Confidence threshold used during prediction.

Example:

```json
"conf": 0.25
```

### 15.2 `iou`

IoU threshold used during prediction or evaluation filtering.

Example:

```json
"iou": 0.45
```

### 15.3 YOLO metrics

Supported YOLO metrics:

```text
map50
map50_95
precision
recall
```

---

## 16. Complete example — JSON architecture with `.npz`

```json
{
  "port": 9000,
  "backlog": 10,
  "defense": false,
  "dataset": {
    "type": "npz",
    "train_path": "input_server/dataset_train.npz",
    "test_path": "input_server/dataset_test.npz",
    "target_dtype": "long"
  },
  "model": {
    "id": 0,
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
        "base_dir": "output_server/globalResults",
        "print_to_console": true,
        "save_results": true,
        "use_timestamp_path": true
      }
    }
  }
}
```

---

## 17. Complete example — generic PyTorch `.pt` with `.npz`

```json
{
  "port": 9000,
  "backlog": 10,
  "defense": false,
  "dataset": {
    "type": "npz",
    "train_path": "input_server/dataset_train.npz",
    "test_path": "input_server/dataset_test.npz",
    "target_dtype": "long"
  },
  "model": {
    "id": 0,
    "pt_path": "input_server/model.pt"
  },
  "training": {
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
        "base_dir": "output_server/globalResults",
        "print_to_console": true,
        "save_results": true,
        "use_timestamp_path": true
      }
    }
  }
}
```

This is not YOLO. It is classic training with a `.pt` model.

---

## 18. Complete example — YOLO `.pt`

```json
{
  "port": 9000,
  "backlog": 10,
  "defense": false,
  "dataset": {
    "type": "yolo",
    "train_path": "input_server/dataset",
    "val_path": "input_server/dataset/val/images",
    "nc": 1,
    "names": ["pina"]
  },
  "model": {
    "id": 0,
    "pt_path": "input_server/model.pt"
  },
  "training": {
    "epochs": 3,
    "batch_size": 8,
    "imgsz": 640,
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
      "conf": 0.2,
      "iou": 0.4
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
        "base_dir": "output_server/globalResults",
        "print_to_console": true,
        "save_results": true,
        "use_timestamp_path": true
      }
    }
  }
}
```

---

## 19. Practical checklist

Before running the server:

1. Check that `port` matches every client.
2. Check that `.env` exists and contains `SERVER_PASSWORD_HASH`.
3. Check that `dataset.type` matches the experiment.
4. Check that `train_path` exists.
5. Check that `test_path` exists for `.npz` evaluation.
6. Check that YOLO datasets have `train/images` and `train/labels`.
7. Check that `model.pt_path` exists when using `.pt` mode.
8. Check that YOLO mode has `dataset.nc` and `dataset.names`.
9. Check that `evaluation.task_type` matches the dataset/task.
10. In JSON architecture mode, use menu option 3 if `weights` is empty.
11. In generic `.pt` mode, make sure the model uses supported layer types.
12. In YOLO mode, make sure clients also use YOLO `pt_path` mode.
