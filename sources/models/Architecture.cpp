#include "models/Architecture.h"

Architecture::Architecture(){
}

Architecture::~Architecture(){
}

void Architecture::addLayer(const Layer& layer){
    layers.push_back(layer);
}  

void Architecture::printArchitecture(){
    std::cout << "Architecture with " << layers.size() << " layers:\n";

    for (size_t i = 0; i < layers.size(); ++i) {

        const Layer& layer = layers[i];

        std::cout << "Layer " << i + 1 << ": ";

        switch(layer.type){

            case NONETYPE:
                std::cout << "NONE";
                break;

            case LAYER_INPUT:
                std::cout << "INPUT "
                          << "(" << layer.output_dim[0] << ", "
                          << layer.output_dim[1] << ", "
                          << layer.output_dim[2] << ")";
                break;

            case LAYER_CONV2D:
                std::cout << "CONV2D "
                          << "kernel=" << layer.kernel_size
                          << " stride=" << layer.stride
                          << " padding=" << layer.padding
                          << " in=(" << layer.input_dim[0] << ", "
                          << layer.input_dim[1] << ", "
                          << layer.input_dim[2] << ")"
                          << " out=(" << layer.output_dim[0] << ", "
                          << layer.output_dim[1] << ", "
                          << layer.output_dim[2] << ")";
                break;

            case LAYER_MAXPOOL2D:
                std::cout << "MAXPOOL2D "
                          << "kernel=" << layer.kernel_size
                          << " stride=" << layer.stride;
                break;

            case LAYER_FLATTEN:
                std::cout << "FLATTEN "
                          << "in=(" << layer.input_dim[0] << ", "
                          << layer.input_dim[1] << ", "
                          << layer.input_dim[2] << ") "
                          << "out=" << layer.out_features;
                break;

            case LAYER_DENSE:
                std::cout << "DENSE "
                          << "(" << layer.in_features
                          << " -> " << layer.out_features << ")";
                break;

            case LAYER_BATCHNORM:
                std::cout << "BATCHNORM";
                break;

            case LAYER_DROPOUT:
                std::cout << "DROPOUT";
                break;

            case LAYER_ACTIVATION:
                std::cout << "ACTIVATION "
                          << layer.activation;
                break;

            default:
                std::cout << "UNKNOWN LAYER";
                break;
        }

        std::cout << "\n";
    }
}
std::vector<Layer> Architecture::getLayers() const{
    return layers;
}