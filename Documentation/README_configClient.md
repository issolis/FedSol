# README — Construcción de `configClient.json`

Este documento describe cómo construir el archivo `configClient.json` para un cliente en el sistema de entrenamiento federado.  
El archivo define:

- identidad del cliente
- conexión con el servidor
- dataset local
- modelo
- entrenamiento

La parte más importante es `model.architecture`, porque ahí se define la estructura exacta de la red que será reconstruida tanto en Python como interpretada por el sistema.

---

## Estructura general

Un `configClient.json` tiene esta forma:

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
        "path": "dataset.npz",
        "type": "npz"
    },
    "model": {
        "architecture": {
            "layers": []
        },
        "id": 1,
        "weights": []
    },
    "training": {
        "batch_size": 32,
        "epochs": 5,
        "loss": {
            "name": "cross_entropy",
            "params": {}
        },
        "lr": 0.001,
        "optimizer": "adam"
    }
}
```

---

## 1. Sección `client`

Define la identidad lógica del nodo cliente.

```json
"client": {
    "id": 1
}
```

### Campos

#### `id`
Identificador único del cliente dentro del sistema.

Debe ser un entero positivo.

Ejemplo:

```json
"id": 1
```

Este valor se usa para distinguir clientes en el servidor, en logs y en la coordinación de rondas.

---

## 2. Sección `connection`

Define cómo se conecta el cliente al servidor federado.

```json
"connection": {
    "password": "federated123",
    "port": 9000,
    "server_ip": "192.168.68.118"
}
```

### Campos

#### `password`
Contraseña compartida para el handshake o autenticación inicial.

#### `port`
Puerto del servidor.

#### `server_ip`
Dirección IP del servidor.

---

## 3. Sección `dataset`

Describe el dataset local del cliente.

```json
"dataset": {
    "path": "dataset.npz",
    "type": "npz"
}
```

### Campos

#### `path`
Ruta al archivo del dataset local.

#### `type`
Tipo de dataset. Actualmente el sistema trabaja con:

```json
"type": "npz"
```

### Formato esperado del `.npz`

El archivo debe contener:

- `X`
- `y`

Ejemplo conceptual:

- `X`: tensor de entrada
- `y`: etiquetas

Para Fashion-MNIST:

- `X` con forma `(N, 1, 28, 28)`
- `y` con forma `(N,)`

---

## 4. Sección `model`

Esta sección define el modelo del cliente.

```json
"model": {
    "architecture": {
        "layers": []
    },
    "id": 1,
    "weights": []
}
```

### Campos

#### `id`
Identificador lógico del modelo.

#### `weights`
Vector plano de pesos del modelo.

Puede contener:
- pesos reales
- o un placeholder inicial, por ejemplo:

```json
"weights": [0.0]
```

#### `architecture`
Describe la arquitectura completa del modelo mediante una lista de capas.

---

## 5. Sección `training`

Define los hiperparámetros de entrenamiento local.

```json
"training": {
    "batch_size": 32,
    "epochs": 5,
    "loss": {
        "name": "cross_entropy",
        "params": {}
    },
    "lr": 0.001,
    "optimizer": "adam"
}
```

### Campos

#### `batch_size`
Tamaño de lote.

#### `epochs`
Cantidad de épocas locales.

#### `loss`
Función de pérdida.

Ejemplo:

```json
"loss": {
    "name": "cross_entropy",
    "params": {}
}
```

#### `lr`
Learning rate.

#### `optimizer`
Optimizador.

Ejemplo:

```json
"optimizer": "adam"
```

---

## 6. Sección `architecture`

Esta es la parte más importante del archivo.

La arquitectura se define como una secuencia ordenada de capas:

```json
"architecture": {
    "layers": [
        { ... },
        { ... },
        { ... }
    ]
}
```

Cada objeto dentro de `layers` representa una capa.

El sistema reconstruye el modelo leyendo las capas en orden.

---

## 7. Tipos de capa soportados

Los tipos de capa se representan con enteros.

### Tabla de tipos

```text
0 -> NONETYPE
1 -> INPUT
2 -> CONV2D
3 -> MAXPOOL2D
4 -> FLATTEN
5 -> DENSE
6 -> BATCHNORM
7 -> DROPOUT
8 -> ACTIVATION
```

Esto corresponde a:

```cpp
enum class LayerType : uint32_t
{
    NONETYPE = 0,
    LAYER_INPUT = 1,
    LAYER_CONV2D = 2,
    LAYER_MAXPOOL2D = 3,
    LAYER_FLATTEN = 4,
    LAYER_DENSE = 5,
    LAYER_BATCHNORM = 6,
    LAYER_DROPOUT = 7,
    LAYER_ACTIVATION = 8
};
```

### Activaciones soportadas

```text
0 -> NONEACT
1 -> ACT_NONE
2 -> ACT_RELU
3 -> ACT_SIGMOID
4 -> ACT_TANH
5 -> ACT_SOFTMAX
```

---

## 8. Estructura de una capa

Cada capa puede incluir varios campos, aunque no todos aplican a todos los tipos.

Forma general:

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

### Campos

#### `type`
Tipo de capa.

#### `input_dim`
Dimensión de entrada en formato:

```json
[alto, ancho, canales]
```

#### `output_dim`
Dimensión de salida en formato:

```json
[alto, ancho, canales]
```

#### `kernel_size`
Tamaño de kernel, usado en convolución o pooling.

#### `stride`
Stride de la operación.

#### `padding`
Padding de la convolución.

#### `in_features`
Cantidad de entradas para una capa densa o flatten.

#### `out_features`
Cantidad de salidas para una capa densa o número de filtros en algunos contextos lógicos del JSON.

#### `activation`
Tipo de activación, solo relevante en capas `ACTIVATION`.

---

## 9. Cómo construir `architecture.layers`

La regla principal es esta:

> cada capa debe ser coherente con la salida de la capa anterior

Eso significa que debes actualizar correctamente dimensiones, features y canales en cada paso.

---

## 10. Capa `INPUT`

Representa la entrada del modelo.

Ejemplo para Fashion-MNIST:

```json
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
```

### Uso
Solo define la forma inicial del dato.

No agrega pesos.

---

## 11. Capa `CONV2D`

Representa una convolución 2D.

Ejemplo:

```json
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
}
```

### Significado

- entrada: imagen `28x28x1`
- salida: `28x28x8`
- kernel: `3x3`
- stride: `1`
- padding: `1`

### Regla importante
Los canales de entrada son `input_dim[2]` y los de salida `output_dim[2]`.

### Pesos esperados
Una convolución aporta:

```text
kernel_size * kernel_size * in_channels * out_channels + out_channels
```

El `+ out_channels` corresponde al bias.

---

## 12. Capa `ACTIVATION`

Representa una activación como capa separada.

Ejemplo ReLU:

```json
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
}
```

### Regla importante
La activación no cambia dimensiones.

### Nota
Si se usa `cross_entropy`, normalmente no se debe poner `softmax` al final del modelo.

---

## 13. Capa `MAXPOOL2D`

Representa un max pooling.

Ejemplo:

```json
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
}
```

### Significado

- reduce resolución espacial
- mantiene canales
- no agrega pesos

---

## 14. Capa `FLATTEN`

Convierte una salida tridimensional en un vector plano.

Ejemplo:

```json
{
    "type": 4,
    "input_dim": [7, 7, 16],
    "output_dim": [1, 1, 1],
    "kernel_size": 0,
    "stride": 0,
    "padding": 0,
    "in_features": 784,
    "out_features": 784,
    "activation": 0
}
```

### Regla importante

Aquí:

```text
in_features = 7 * 7 * 16 = 784
```

Ese valor debe coincidir exactamente.

### Nota
`FLATTEN` no agrega pesos.

---

## 15. Capa `DENSE`

Representa una capa totalmente conectada.

Ejemplo:

```json
{
    "type": 5,
    "input_dim": [1, 1, 1],
    "output_dim": [1, 1, 1],
    "kernel_size": 0,
    "stride": 0,
    "padding": 0,
    "in_features": 784,
    "out_features": 128,
    "activation": 0
}
```

### Significado

- entrada: vector de tamaño 784
- salida: vector de tamaño 128

### Pesos esperados

```text
in_features * out_features + out_features
```

El `+ out_features` es el bias.

---

## 16. Capas `BATCHNORM` y `DROPOUT`

Estas capas están soportadas en el sistema y pueden usarse dentro de la arquitectura.

---

### Capa `BATCHNORM`

Representa una normalización por batch en 2D.

#### Ejemplo

```json
{
    "type": 6,
    "input_dim": [28, 28, 8],
    "output_dim": [28, 28, 8],
    "kernel_size": 0,
    "stride": 0,
    "padding": 0,
    "in_features": 0,
    "out_features": 0,
    "activation": 0
}
```

#### Significado

- Aplica normalización sobre los canales
- No cambia las dimensiones espaciales
- No cambia el número de canales

#### Regla importante

El número de canales se obtiene en este orden:

1. `output_dim[2]`
2. `input_dim[2]`
3. `out_features`

Al menos uno de estos debe estar definido.

#### Implementación en PyTorch

```python
nn.BatchNorm2d(channels)
```

---

### Capa `DROPOUT`

Representa una capa de regularización que desactiva neuronas de forma aleatoria durante el entrenamiento.

#### Ejemplo

```json
{
    "type": 7,
    "input_dim": [1, 1, 128],
    "output_dim": [1, 1, 128],
    "kernel_size": 0,
    "stride": 0,
    "padding": 0,
    "in_features": 0,
    "out_features": 0,
    "activation": 0
}
```

#### Significado

- Desactiva aleatoriamente un porcentaje de neuronas durante el entrenamiento (`p = 0.5`)
- Introduce ruido controlado para evitar overfitting
- No cambia las dimensiones del tensor

#### Comportamiento

- En entrenamiento (`model.train()`):
  - Se aplica Dropout
- En evaluación (`model.eval()`):
  - Se desactiva automáticamente

#### Implementación en PyTorch

```python
nn.Dropout(p=0.5)
```

#### Uso típico

Después de capas densas:

```text
Flatten → Dense → ReLU → Dropout → Dense
```

También puede usarse en redes convolucionales:

```text
Conv → ReLU → Dropout → Pool
```

#### Buenas prácticas

- Usarlo después de capas `DENSE`
- No colocarlo en la capa de salida
- Valores comunes:
  - `p = 0.5` (estándar)
  - `p = 0.2 - 0.3` para redes pequeñas

#### Consideraciones

- No agrega pesos
- No cambia dimensiones
- Solo afecta el entrenamiento
- Se desactiva automáticamente en evaluación

## 17. Ejemplo completo de arquitectura CNN simple

Ejemplo típico para Fashion-MNIST:

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

---

## 18. Reglas prácticas para diseñar arquitecturas

### Regla 1
La salida de una capa debe coincidir con la entrada lógica de la siguiente.

### Regla 2
Las capas `INPUT`, `MAXPOOL`, `FLATTEN` y `ACTIVATION` no agregan pesos.

### Regla 3
Las capas que sí agregan pesos son principalmente:

- `CONV2D`
- `DENSE`

### Regla 4
Si se usa `FLATTEN`, calcula bien:

```text
alto * ancho * canales
```

### Regla 5
La arquitectura debe ser idéntica en cliente y servidor.

---

## 19. Errores comunes

### `Flatten` mal calculado
Si el número de `in_features` no coincide con la salida real anterior, la siguiente `Dense` fallará.

### `output_dim` incorrecto
Si después de un `Conv2D` o `MaxPool2D` defines mal la salida, el resto de la red queda inconsistente.

### Softmax final con `cross_entropy`
No suele ser correcto agregar `ACT_SOFTMAX` al final si ya usas `CrossEntropyLoss`.

### Pesos incompatibles con arquitectura
Si cambias la arquitectura pero mantienes un vector de pesos viejo, el conteo deja de coincidir.

---

## 20. Recomendación práctica

Para construir un `configClient.json` correctamente:

1. define el dataset local  
2. define la forma de entrada  
3. construye la arquitectura capa por capa  
4. verifica las dimensiones después de cada operación  
5. calcula bien `flatten`  
6. asegúrate de que la última `Dense` tenga el número correcto de salidas  
7. usa la misma arquitectura en todos los nodos

---

## 21. Resumen

El `configClient.json` describe todo lo necesario para que un cliente:

- se conecte al servidor
- cargue su dataset
- reconstruya el modelo
- entrene localmente
- trabaje con la misma arquitectura que el resto del sistema

La parte crítica es `model.architecture.layers`, porque esa sección define el contrato estructural del modelo.
