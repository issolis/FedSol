#ifndef FLCOORDINATOR_H
#define FLCOORDINATOR_H

#include <vector>
#include <thread>
#include <mutex>
#include "models/Model.h"
#include "protocol/protocol.h"
#include "FedAvg/FedAvg.h"
#include "network/sharedState.h"

class FLCoordinator
{
private:
    SharedState& shared;
    Model& globalModel;
    std::mutex modelMutex;

public:
    FLCoordinator(SharedState& shared, Model& model);

    void startTraining();
    void aggregate(bool endTraining);
};

#endif