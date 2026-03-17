#include "models/Layer.h"

Layer::Layer(){
    this->type = LayerType::NONETYPE; 
    this->input_dim[0] = 0; 
    this->input_dim[1] = 0;
    this->input_dim[2] = 0;
    this->output_dim[0] = 0;
    this->output_dim[1] = 0;
    this->output_dim[2] = 0;
    this->kernel_size = 0;           
    this->stride = 0;
    this->padding = 0;
    this->in_features = 0;
    this->out_features = 0;
    this->activation = ActivationType::NONEACT;
}

bool Layer::operator==(const Layer& other) const
{
    if(type != other.type)
        return false;

    for(int i = 0; i < 3; i++)
        if(input_dim[i] != other.input_dim[i])
            return false;

    for(int i = 0; i < 3; i++)
        if(output_dim[i] != other.output_dim[i])
            return false;

    if(kernel_size != other.kernel_size)
        return false;

    if(stride != other.stride)
        return false;

    if(padding != other.padding)
        return false;

    if(in_features != other.in_features)
        return false;

    if(out_features != other.out_features)
        return false;

    if(activation != other.activation)
        return false;

    return true;
}