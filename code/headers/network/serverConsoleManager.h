#ifndef SERVERCONSOLEMANGER_H
#define SERVERCONSOLEMANAGER_H

#include <iostream>
#include "logger/logger.h"
#include "weightsUtils/weightsUtils.h"
#include "models/Model.h"
#include "federated/trainer.h"
#include <string>
#include "jsonManager/jsonManager.h"
#include "federated/sharedState.h"

class ServerConsoleManager
{
private:
    /* data */
public:
    static int menu();
    static void handleStartTraining(const Model &globalModel, SharedState &shared, Trainer &trainer, const std::string &ptHash = "");
    static void handlePopulateModel(Model& globalModel, const std::string & path); 
    static void handleShowClients(SharedState &share); 
    static void handleEvaluation(const std::string& configPath, const std::string& scriptPath); 
    static void handleSplitDataset(const std::string& datasetpath, SharedState &share); 
    static void handleStopClients(SharedState &shared);
};

#endif