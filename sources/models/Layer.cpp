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