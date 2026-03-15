#include <iostream>
#include "models/Model.h"
#include "models/Architecture.h"
#include "serializer/modelSerializer.h"

int main()
{
    std::cout << "=== TEST SERIALIZER ===\n\n";

    // ------------------------------------------------
    // 1. Crear arquitectura
    // ------------------------------------------------

    Architecture arch;

    Layer input;
    input.type = LayerType::LAYER_INPUT;
    input.output_dim[0] = 28;
    input.output_dim[1] = 28;
    input.output_dim[2] = 1;

    Layer conv;
    conv.type = LayerType::LAYER_CONV2D;
    conv.kernel_size = 3;
    conv.stride = 1;
    conv.padding = 1;
    conv.input_dim[0] = 28;
    conv.input_dim[1] = 28;
    conv.input_dim[2] = 1;
    conv.output_dim[0] = 28;
    conv.output_dim[1] = 28;
    conv.output_dim[2] = 32;

    Layer dense;
    dense.type = LayerType::LAYER_DENSE;
    dense.in_features = 5408;
    dense.out_features = 10;

    arch.addLayer(input);
    arch.addLayer(conv);
    arch.addLayer(dense);

    std::cout << "Original architecture:\n";
    arch.printArchitecture();
    std::cout << "\n";

    // ------------------------------------------------
    // 2. Crear weights
    // ------------------------------------------------

    std::vector<uint32_t> weights;

    for (uint32_t i = 0; i < 10; i++)
        weights.push_back(i * 10);

    // ------------------------------------------------
    // 3. Crear modelo
    // ------------------------------------------------

    Model model(1, arch, weights);

    // ------------------------------------------------
    // 4. Serializar
    // ------------------------------------------------

    auto serialized = ModelSerializer::serialize(model);

    std::vector<char> posBuffer = serialized.first;
    std::vector<char> modelBuffer = serialized.second;

    std::cout << "Serialized sizes:\n";
    std::cout << "posBuffer: " << posBuffer.size() << "\n";
    std::cout << "modelBuffer: " << modelBuffer.size() << "\n\n";

    // ------------------------------------------------
    // 5. Deserializar
    // ------------------------------------------------

    Model recovered =
        ModelSerializer::deserialize(posBuffer, modelBuffer);

    // ------------------------------------------------
    // 6. Verificar
    // ------------------------------------------------

    std::cout << "Recovered architecture:\n";

    recovered.getArchitecture().printArchitecture();

    std::cout << "\nRecovered weights:\n";

    for (uint32_t w : recovered.getWeights())
        std::cout << w << " ";

    std::cout << "\n\nTEST COMPLETED\n";

    return 0;
}