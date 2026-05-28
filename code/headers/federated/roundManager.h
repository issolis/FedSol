#ifndef ROUNDMANAGER_H
#define ROUNDMANAGER_H

#include <string>
#include <cstdint>
#include "federated/sharedState.h"
#include "federated/trainer.h"
#include "federated/aggregator.h"

class RoundManager
{
private:
    SharedState &shared;
    Trainer &trainer;
    Aggregator &aggregator;
    std::string path;

    bool markClientAsFinished(uint32_t id);
    void registerClientSampleSize(uint32_t id, uint32_t sampleSize);

public:
    RoundManager(
        SharedState &shared,
        Trainer &trainer,
        Aggregator &aggregator);

    void tryAggregateAndContinue();
    void handleTrainingFinished(int clientSockID, const std::string &path);
    
};

#endif