#ifndef LOCALTRAINING_H 
#define LOCALTRAINING_H 
#include <string>
#include <iostream>
#include "jsonManager/jsonManager.h"
#include "models/Model.h"
#include "protocol/protocol.h"
#include "network/connection.h"
#include "logger/logger.h"


class LocalTraining
{
public:
   static void startLocalTraining(const std::string &path, Model& model); 
   static void reportTrainingEnded(const std::string& serverIP, const std::string& password, const std::string &path, uint16_t port, Model& model);
};


#endif