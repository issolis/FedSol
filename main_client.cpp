#include <iostream>
#include <vector>

#include "models/Model.h"
#include "models/Architecture.h"
#include "models/Layer.h"
#include "network/client.h"

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
        weights.push_back(i);

    // -------- CLIENT MODEL --------
    Model clientModel(1, arch, weights);

    std::vector<char> modelBuffer = clientModel.serialize();
    std::vector<char> posBuffer = clientModel.serializePos();

    std::cout << "Model bytes: " << modelBuffer.size() << std::endl;
    std::cout << "Pos bytes: " << posBuffer.size() << std::endl;

    // -------- BUILD FINAL BUFFER --------
    std::vector<char> sendBuffer;

    uint32_t posSize = posBuffer.size();
    uint32_t modelSize = modelBuffer.size();

    
    sendBuffer.insert(sendBuffer.end(),
                      (char*)&posSize,
                      (char*)&posSize + sizeof(posSize));

    sendBuffer.insert(sendBuffer.end(),
                      posBuffer.begin(),
                      posBuffer.end());

    sendBuffer.insert(sendBuffer.end(),
                      (char*)&modelSize,
                      (char*)&modelSize + sizeof(modelSize));

    sendBuffer.insert(sendBuffer.end(),
                      modelBuffer.begin(),
                      modelBuffer.end());

    // -------- CLIENT --------
    Client client(9000, "127.0.0.1");

    std::cout<< "Bytes sent " << sendBuffer.size() << std::endl; 
    client.send(sendBuffer);

    

    return 0;
}