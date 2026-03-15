#ifndef MODELSERIALIZER_H
#define MODELSERIALIZER_H
#include <utility>
#include <vector>
#include "models/Model.h"

class ModelSerializer
{
private:


public:
    static std::pair<std::vector<char>, std::vector<char>> serialize(Model& model); 
    static Model deserialize(const std::vector<char>& serializedPos, const std::vector<char>& serializedModel); 

    static std::vector<char> serializePos(const std::vector<uint32_t>& nonSerializedPos);
    static std::vector<uint32_t> deserializePos(const std::vector<char>& buffer); 
   
};


#endif