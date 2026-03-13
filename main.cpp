#include <iostream>
#include <vector>

#include "models/Model.h"
#include "models/Architecture.h"
#include "models/Layer.h"

int main()
{
    // -------- CREATE ARCHITECTURE --------
    Architecture arch;

    Layer input;
    input.type = LAYER_INPUT;
    input.output_dim[0] = 28;
    input.output_dim[1] = 28;
    input.output_dim[2] = 1;

    arch.addLayer(input);

    Layer dense;
    dense.type = LAYER_DENSE;
    dense.in_features = 784;
    dense.out_features = 10;

    arch.addLayer(dense);

    // -------- CREATE WEIGHTS --------
    std::vector<uint32_t> weights;

    for(uint32_t i = 0; i < 10; i++)
    {
        weights.push_back(i);
    }

    // -------- CLIENT MODEL --------
    Model clientModel(1, arch, weights);

    std::vector<char> modelBuffer = clientModel.serializeModel();
    std::vector<char> posBuffer = clientModel.serializePos();

    std::cout << "Model bytes: " << modelBuffer.size() << std::endl;
    std::cout << "Pos bytes: " << posBuffer.size() << std::endl;

    // -------- SERVER SIDE --------
    Model serverModel;

    serverModel.setSerializedPos(posBuffer);

    Model reconstructed = serverModel.deserializeModel(modelBuffer);

    // -------- PRINT RESULTS --------
    std::cout << "\nRecovered positions:\n";

    std::vector<uint32_t> pos = reconstructed.getNonSerializedPos();

    for(size_t i = 0; i < pos.size(); i++)
    {
        std::cout << "pos[" << i << "] = " << pos[i] << std::endl;
    }

    std::cout << "\nRecovered architecture:\n";
    reconstructed.getArchitecture().printArchitecture();

    return 0;
}