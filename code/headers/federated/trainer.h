#ifndef TRAINER_H
#define TRAINER_H

#include "federated/sharedState.h"
#include "models/Model.h"

class Trainer
{
private:
    SharedState& shared;
    Model& globalModel;

public:
    Trainer(SharedState& shared, Model& globalModel);

    void startTraining();
};

#endif