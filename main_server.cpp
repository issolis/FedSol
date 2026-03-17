#include <iostream>

#include "network/server.h"

int main()
{
    unsigned short port = 9000;
    uint32_t backlog = 10;

    Architecture arch;

    // -------- INPUT --------
    Layer input;
    input.type = LayerType::LAYER_INPUT;
    input.output_dim[0] = 32;
    input.output_dim[1] = 32;
    input.output_dim[2] = 3;
    arch.addLayer(input);

    // -------- CONV BLOCK 1 --------
    Layer conv1;
    conv1.type = LayerType::LAYER_CONV2D;
    conv1.kernel_size = 3;
    conv1.stride = 1;
    conv1.padding = 1;
    conv1.input_dim[0] = 32;
    conv1.input_dim[1] = 32;
    conv1.input_dim[2] = 3;
    conv1.output_dim[0] = 32;
    conv1.output_dim[1] = 32;
    conv1.output_dim[2] = 32;
    arch.addLayer(conv1);

    Layer act1;
    act1.type = LayerType::LAYER_ACTIVATION;
    act1.activation = ActivationType::ACT_RELU;
    arch.addLayer(act1);

    Layer pool1;
    pool1.type = LayerType::LAYER_MAXPOOL2D;
    pool1.kernel_size = 2;
    pool1.stride = 2;
    arch.addLayer(pool1);

    // -------- CONV BLOCK 2 --------
    Layer conv2;
    conv2.type = LayerType::LAYER_CONV2D;
    conv2.kernel_size = 3;
    conv2.stride = 1;
    conv2.padding = 1;
    conv2.input_dim[0] = 16;
    conv2.input_dim[1] = 16;
    conv2.input_dim[2] = 32;
    conv2.output_dim[0] = 16;
    conv2.output_dim[1] = 16;
    conv2.output_dim[2] = 64;
    arch.addLayer(conv2);

    Layer act2;
    act2.type = LayerType::LAYER_ACTIVATION;
    act2.activation = ActivationType::ACT_RELU;
    arch.addLayer(act2);

    Layer pool2;
    pool2.type = LayerType::LAYER_MAXPOOL2D;
    pool2.kernel_size = 2;
    pool2.stride = 2;
    arch.addLayer(pool2);

    // -------- CONV BLOCK 3 --------
    Layer conv3;
    conv3.type = LayerType::LAYER_CONV2D;
    conv3.kernel_size = 3;
    conv3.stride = 1;
    conv3.padding = 1;
    conv3.input_dim[0] = 8;
    conv3.input_dim[1] = 8;
    conv3.input_dim[2] = 64;
    conv3.output_dim[0] = 8;
    conv3.output_dim[1] = 8;
    conv3.output_dim[2] = 128;
    arch.addLayer(conv3);

    Layer act3;
    act3.type = LayerType::LAYER_ACTIVATION;
    act3.activation = ActivationType::ACT_RELU;
    arch.addLayer(act3);

    // -------- FLATTEN --------
    Layer flatten;
    flatten.type = LayerType::LAYER_FLATTEN;
    flatten.input_dim[0] = 8;
    flatten.input_dim[1] = 8;
    flatten.input_dim[2] = 128;
    flatten.out_features = 8192;
    arch.addLayer(flatten);

    // -------- DENSE BLOCK --------
    Layer dense1;
    dense1.type = LayerType::LAYER_DENSE;
    dense1.in_features = 8192;
    dense1.out_features = 1024;
    arch.addLayer(dense1);

    Layer act4;
    act4.type = LayerType::LAYER_ACTIVATION;
    act4.activation = ActivationType::ACT_RELU;
    arch.addLayer(act4);

    Layer dense2;
    dense2.type = LayerType::LAYER_DENSE;
    dense2.in_features = 1024;
    dense2.out_features = 256;
    arch.addLayer(dense2);

    Layer act5;
    act5.type = LayerType::LAYER_ACTIVATION;
    act5.activation = ActivationType::ACT_RELU;
    arch.addLayer(act5);

    Layer dense3;
    dense3.type = LayerType::LAYER_DENSE;
    dense3.in_features = 256;
    dense3.out_features = 10;
    arch.addLayer(dense3);

    Layer output;
    output.type = LayerType::LAYER_ACTIVATION;
    output.activation = ActivationType::ACT_SOFTMAX;
    arch.addLayer(output);

    // -------- CREATE WEIGHTS --------
    std::vector<uint32_t> weights;

    for (uint32_t i = 0; i < 10; i++)
        weights.push_back(i);

    Model serverModel(0, arch, weights); 


    Server server(port, backlog, serverModel);

    std::cout << "Starting server..." << std::endl;

    server.run();

    return 0;
}