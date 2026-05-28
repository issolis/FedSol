import json
import torch
import torch.nn as nn
from datetime import datetime
import os 

class ModelBuilder:

    def __init__(self,path):
        self.path = path
        self.layers = []
        self.weights = []

    def load(self): 
        with open(self.path, "r") as f:
            data = json.load(f)

        model_data = data["model"]

        self.arch = model_data["architecture"]["layers"]
        self.weights = model_data["weights"]
        
    def load_weights(self, model):
        if len(self.weights) <= 1:
            print("[WARNING] No valid weights found")
            return

        flat = torch.tensor(self.weights, dtype=torch.float32)
        ptr = 0

        # Support both nn.Sequential and arbitrary model architectures
        if isinstance(model, nn.Sequential):
            modules = list(model)
        else:
            # For complex models, iterate only leaf modules that have parameters
            modules = [m for m in model.modules()
                       if len(list(m.children())) == 0 and len(list(m.parameters(recurse=False))) > 0]

        for layer in modules:

            # -------- CONV2D --------
            if isinstance(layer, nn.Conv2d):
                weight_size = layer.weight.numel()

                layer.weight.data = flat[ptr:ptr+weight_size].view(layer.weight.shape)
                ptr += weight_size

                if layer.bias is not None:
                    bias_size = layer.bias.numel()
                    layer.bias.data = flat[ptr:ptr+bias_size]
                    ptr += bias_size

            # -------- LINEAR --------
            elif isinstance(layer, nn.Linear):
                weight_size = layer.weight.numel()

                layer.weight.data = flat[ptr:ptr+weight_size] \
                    .view(layer.weight.shape)
                ptr += weight_size

                if layer.bias is not None:
                    bias_size = layer.bias.numel()
                    layer.bias.data = flat[ptr:ptr+bias_size]
                    ptr += bias_size

            # -------- BATCHNORM --------
            elif isinstance(layer, (nn.BatchNorm2d, nn.BatchNorm1d)):
                channels = layer.weight.numel()

                layer.weight.data = flat[ptr:ptr+channels]
                ptr += channels

                layer.bias.data = flat[ptr:ptr+channels]
                ptr += channels

            else:
                continue

        if ptr != len(flat):
            raise ValueError(
                f"Weight mismatch: used {ptr}, total {len(flat)}"
            )
   
    def save_model(self, model, folder="output_server/models"):
        # Crear carpeta si no existe
        os.makedirs(folder, exist_ok=True)

        # Generar timestamp
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

        # Nombre del archivo
        filename = f"model_{timestamp}.pt"
        path = os.path.join(folder, filename)

        # Guardar modelo
        torch.save(model, path)

        return path

    def build(self): 
        self.load()
        self.layers = []  

        for layer in self.arch: 
            t = layer["type"]

            #INPUT
            if t == 1: 
                continue

            #CONV 2D
            elif t == 2: 
                self.layers.append(
                    nn.Conv2d(
                        in_channels=layer["input_dim"][2],
                        out_channels=layer["output_dim"][2],
                        kernel_size=layer["kernel_size"],
                        stride=layer["stride"],
                        padding=layer["padding"]
                    )
                )

            #MAXPOOl2D
            elif t == 3: 
                self.layers.append(
                    nn.MaxPool2d(kernel_size=2)
                )

            #FLATTEN
            elif t == 4:
                self.layers.append(nn.Flatten())


            #DENSE
            elif t == 5: 
                self.layers.append(
                    nn.Linear(
                        layer["in_features"],
                        layer["out_features"]
                    )
                )

            #BATCHNORM
            elif t == 6:
                channels = None

                if "output_dim" in layer:
                    channels = layer["output_dim"][2]
                elif "input_dim" in layer:
                    channels = layer["input_dim"][2]
                elif "out_features" in layer:
                    channels = layer["out_features"]

                if channels is None:
                    raise ValueError("Invalid BatchNorm layer")

                self.layers.append(nn.BatchNorm2d(channels))

            #DROPOUT
            elif t == 7:
                self.layers.append(
                    nn.Dropout(p=0.5)
                )

            #ACTIVATION
           # -------- ACTIVATION --------
            # -------- ACTIVATION --------
            elif t == 8:  # LAYER_ACTIVATION
                act = layer.get("activation", 0)

                activation_map = {
                    2: lambda: nn.ReLU(),           # ACT_RELU
                    3: lambda: nn.Sigmoid(),        # ACT_SIGMOID
                    4: lambda: nn.Tanh(),           # ACT_TANH
                    5: lambda: nn.Softmax(dim=1),   # ACT_SOFTMAX
                    0: lambda: nn.Identity(),       # NONEACT
                    1: lambda: nn.Identity(),       # ACT_NONE
                }

                self.layers.append(
                    activation_map.get(act, lambda: nn.Identity())()
                )

            else:
                raise ValueError(f"Unknown layer type {t}")

        model = nn.Sequential(*self.layers)

        self.load_weights(model)

        return model