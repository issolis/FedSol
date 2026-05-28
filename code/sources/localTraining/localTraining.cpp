#include "localTraining/localTraining.h"
#include "datasetutils/datasetutils.h"
#include "protocol/OP_CODES.h"

void LocalTraining::startLocalTraining(const std::string &path, Model &model)
{
    std::cout << "[INFO] Starting local training...\n";
    std::string command =
        "PYTHONPATH=code python3 -m FedSolPython.scripts.train \"" + path + "\"";

    int result = system(command.c_str());

    if (result == -1)
    {
        std::cout << "[ERROR] Failed to execute command\n";
        return;
    }

    int exitCode = WEXITSTATUS(result);

    if (exitCode != 0)
    {
        std::cout << "[ERROR] Training failed with code: "
                  << exitCode << "\n";
        return;
    }

    std::cout << "[INFO] Training completed successfully\n";

    auto weights = JSONManager::getWeightsFromJSON(path);
    model.setWeights(weights);
}

void LocalTraining::reportTrainingEnded(const std::string &serverIP, const std::string &password, const std::string &path, uint16_t port, Model &model)
{
    int sockID = Connection::createConnection(serverIP, port);
    uint32_t samples = DatasetUtils::getTotalSamplesFromNPZ(path, "get_total_samples");

    Protocol::sendAuthMessage(sockID, AuthOp::TRAINING_FINISHED, password, "Training Ended");
    Protocol::sendID(sockID, model.getID());
    Protocol::sendSamplesSize(sockID, samples);

    Logger::log(LogLevel::INFO,
                "[Client " + std::to_string(model.getID()) + "] Finished training");

    close(sockID);
}