#include "models/Architecture.h"
#include <iostream>

Architecture::Architecture(){
}

Architecture::~Architecture(){
}

void Architecture::addLayer(const Layer& layer){
    layers.push_back(layer);
}

void Architecture::printArchitecture() const{

    std::cout << "Architecture with " << layers.size() << " layers:\n";

    for (size_t i = 0; i < layers.size(); ++i) {

        const Layer& layer = layers[i];

        std::cout << "Layer " << i + 1 << ": ";

        switch(layer.type){

            case LayerType::NONETYPE:
                std::cout << "NONE";
                break;

            case LayerType::LAYER_INPUT:
                std::cout << "INPUT "
                          << "(" << layer.output_dim[0] << ", "
                          << layer.output_dim[1] << ", "
                          << layer.output_dim[2] << ")";
                break;

            case LayerType::LAYER_CONV2D:
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

            case LayerType::LAYER_MAXPOOL2D:
                std::cout << "MAXPOOL2D "
                          << "kernel=" << layer.kernel_size
                          << " stride=" << layer.stride;
                break;

            case LayerType::LAYER_FLATTEN:
                std::cout << "FLATTEN "
                          << "in=(" << layer.input_dim[0] << ", "
                          << layer.input_dim[1] << ", "
                          << layer.input_dim[2] << ") "
                          << "out=" << layer.out_features;
                break;

            case LayerType::LAYER_DENSE:
                std::cout << "DENSE "
                          << "(" << layer.in_features
                          << " -> " << layer.out_features << ")";
                break;

            case LayerType::LAYER_BATCHNORM:
                std::cout << "BATCHNORM";
                break;

            case LayerType::LAYER_DROPOUT:
                std::cout << "DROPOUT";
                break;

            case LayerType::LAYER_ACTIVATION:
                std::cout << "ACTIVATION "
                          << static_cast<uint32_t>(layer.activation);
                break;

            default:
                std::cout << "UNKNOWN LAYER";
                break;
        }

        std::cout << "\n";
    }
}

const std::vector<Layer>& Architecture::getLayers() const{
    return layers;
}

bool Architecture::equals(const Architecture& other) const
{
    const std::vector<Layer>& layers1 = this->getLayers();
    const std::vector<Layer>& layers2 = other.getLayers();

    if(layers1.size() != layers2.size())
        return false;

    for(size_t i = 0; i < layers1.size(); i++)
    {
        if(!(layers1[i] == layers2[i]))
            return false;
    }

    return true;
}