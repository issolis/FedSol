#include "weightsUtils/weightsUtils.h"
#include "models/Architecture.h"
#include <stdexcept>
#include <random>
#include <iostream>

size_t WeightUtils::computeExpectedWeights(const Architecture& arch)
{
    size_t total = 0;
    int i = 0; 
    arch.printArchitecture(); 
    for (const Layer& layer : arch.getLayers())
    {
        
        switch (layer.type)
        {
            case LayerType::LAYER_INPUT:
                break;

            case LayerType::LAYER_CONV2D:
            {
                int in_channels = layer.input_dim[2];
                int out_channels = layer.output_dim[2];
                int k = layer.kernel_size;

                if (in_channels <= 0 || out_channels <= 0 || k <= 0)
                    throw std::runtime_error("Invalid CONV2D layer while computing weights");

                total += static_cast<size_t>(k) * k * in_channels * out_channels;
                total += static_cast<size_t>(out_channels); // bias
                break;
            }

            case LayerType::LAYER_MAXPOOL2D:
                break;

            case LayerType::LAYER_FLATTEN:
                break;

            case LayerType::LAYER_DENSE:
            {
                int in_features = layer.in_features;
                int out_features = layer.out_features;

                if (in_features <= 0 || out_features <= 0)
                    throw std::runtime_error("Invalid DENSE layer while computing weights");

                total += static_cast<size_t>(in_features) * out_features;
                total += static_cast<size_t>(out_features); // bias
                break;
            }

            case LayerType::LAYER_BATCHNORM:
            {
                int channels = 0;

                if (layer.output_dim[2] > 0)
                    channels = layer.output_dim[2];
                else if (layer.input_dim[2] > 0)
                    channels = layer.input_dim[2];
                else if (layer.out_features > 0)
                    channels = layer.out_features;
                else if (layer.in_features > 0)
                    channels = layer.in_features;

                if (channels <= 0)
                    throw std::runtime_error("Invalid BATCHNORM layer while computing weights");

                total += static_cast<size_t>(2) * channels; // gamma + beta
                break;
            }

            case LayerType::LAYER_DROPOUT:
                break;

            case LayerType::LAYER_ACTIVATION:
                break;

            default:
                throw std::runtime_error("Unknown layer type while computing weights");
        }
    }

    return total;
}

bool WeightUtils::validateWeights(const Architecture& arch, const std::vector<float>& weights)
{
    return weights.size() == computeExpectedWeights(arch);
}

std::vector<float> WeightUtils::generateRandomWeights(size_t size)
{
    std::vector<float> weights(size);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 0.05f);

    for (float& w : weights)
        w = dist(gen);

    return weights;
}