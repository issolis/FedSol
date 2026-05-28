#ifndef AGGREGATOR_H
#define AGGREGATOR_H

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <string>

#include "protocol/protocol.h"
#include "jsonManager/jsonManager.h"
#include "models/Model.h"
#include "federated/FedAvg.h"
#include "federated/modelExporter.h"
#include "federated/sharedState.h"
#include "logger/logger.h"
#include "security/backdoorDefense.h"


class Aggregator
{
private:
    SharedState& shared;
    Model& globalModel;

public:
    Aggregator(SharedState& shared, Model& globalModel);
    bool aggregate(const std::string& path);
};

#endif