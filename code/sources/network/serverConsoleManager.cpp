#include "network/serverConsoleManager.h"
#include "evaluation/evaluationRunner.h"
#include "protocol/serverProtocol.h"
#include "protocol/OP_CODES.h"

int ServerConsoleManager::menu()
{
    int option;

    std::cout << "\n===== SERVER MENU =====" << std::endl;
    std::cout << "1. Start Training" << std::endl;
    std::cout << "2. List Clients" << std::endl;
    std::cout << "3. Populate Random Weights" << std::endl;
    std::cout << "4. Evaluate Model" << std::endl;
    std::cout << "5. Send train data" << std::endl;
    std::cout << "6. Stop client(s)" << std::endl;
    std::cout << "7. Exit" << std::endl;
    std::cout << "Option: ";

    if (!(std::cin >> option))
    {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cout << "[ERROR] Invalid input." << std::endl;
        return -1;
    }

    std::cin.ignore(10000, '\n');

    if (option < 1 || option > 7)
    {
        std::cout << "[ERROR] Option out of range." << std::endl;
        return -1;
    }

    return option;
}

void ServerConsoleManager::handleStartTraining(const Model &globalModel, SharedState &shared, Trainer &trainer, const std::string &ptHash)
{
    if (shared.trainingActive.load())
    {
        std::cout << "[WARNING] Training is already active." << std::endl;
        Logger::log(LogLevel::WARNING, "Training start ignored because training is already active.");
        return;
    }

    if (ptHash.empty())
    {
        bool validWeights = WeightUtils::validateWeights(
            globalModel.getArchitecture(),
            globalModel.getWeights());
 
        if (!validWeights)
        {
            auto [expected, valid] = WeightUtils::computeExpectedWeights(
                globalModel.getArchitecture());
 
            std::cout << "[WARNING] Cannot start training." << std::endl;
            std::cout << "[WARNING] Weights do not match architecture." << std::endl;
            std::cout << "[WARNING] Expected: " << expected << std::endl;
            std::cout << "[WARNING] Current: " << globalModel.getWeights().size() << std::endl;
            std::cout << "[WARNING] Use option 3 to populate random weights first." << std::endl;
 
            return;
        }
    }
    else
    {
        Logger::log(LogLevel::INFO, "[SERVER] pt_path mode — weight validation skipped.");
        std::cout << "[INFO] pt_path mode — weight validation skipped." << std::endl;
    }

    uint32_t epochs = 0;
    std::cout << "Number of epochs: ";

    if (!(std::cin >> epochs) || epochs == 0)
    {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        Logger::log(LogLevel::ERROR, "Invalid epoch value.");
        return;
    }

    char confirm;
    std::cout << "Start training with " << epochs << " epochs? (y/n): ";
    std::cin >> confirm;

    if (confirm == 'y' || confirm == 'Y')
    {
        Logger::log(LogLevel::INFO, "Starting training...");
        std::cout << "[INFO] Starting training..." << std::endl;

        shared.epochs.store(epochs);
        shared.aggregationStarted = false;
        trainer.startTraining();
    }
    else
    {
        std::cout << "Training cancelled." << std::endl;
        Logger::log(LogLevel::INFO, "Training cancelled.");
    }
}

void ServerConsoleManager::handleShowClients(SharedState &share)
{
    share.printClientsStatus();
}

void ServerConsoleManager::handleEvaluation(const std::string &configPath, const std::string &scriptPath)
{
    bool ok = EvaluationRunner::runEvaluation(configPath, scriptPath);
    if (!ok)
    {
        Logger::log(LogLevel::ERROR, "[SERVER] Evaluation failed");
    }
}

void ServerConsoleManager::handleSplitDataset(const std::string &datasetPath, SharedState &shared)
{

    if (shared.clientCount() == 0)
    {
        std::cout << "[SERVER] No clients connected. Aborting." << std::endl;
        return;
    }

    std::cout << "Dataset path: " << datasetPath << std::endl;
    std::cout << "Clients connected: " << shared.clientCount() << std::endl;
    std::cout << "Are you sure you want to split and send the dataset? (y/n): ";

    char confirm;
    std::cin >> confirm;

    if (confirm != 'y' && confirm != 'Y')
    {
        std::cout << "[SERVER] Operation cancelled." << std::endl;
        return;
    }

    ServerProtocol::sendDataset(datasetPath, shared);
}

void ServerConsoleManager::handleStopClients(SharedState &shared)
{
    if (shared.clientCount() == 0)
    {
        std::cout << "[INFO] No clients connected." << std::endl;
        return;
    }

    shared.printClientsStatus();

    std::cout << "Client ID to stop (0 = all): ";
    uint32_t targetId;

    if (!(std::cin >> targetId))
    {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cout << "[ERROR] Invalid input." << std::endl;
        return;
    }

    std::scoped_lock lock(shared.clientsMutex, shared.statesMutex, shared.samplesMutex);

    for (auto it = shared.clientMap.begin(); it != shared.clientMap.end();)
    {
        if (targetId == 0 || it->first == targetId)
        {
            try
            {
                std::lock_guard<std::mutex> ioLock(shared.socketIOMutex);
                Protocol::sendMessage(it->second, ServerOp::SHUTDOWN, "Shutdown");
            }
            catch (...)
            {
                Logger::log(LogLevel::WARNING,
                            "[Console] Could not send shutdown to client " + std::to_string(it->first));
            }

            Logger::log(LogLevel::INFO,
                        "[Console] Shutdown sent to client " + std::to_string(it->first));

            close(it->second);
            shared.statesMap.erase(it->first);
            shared.samplesMap.erase(it->first);
            it = shared.clientMap.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void ServerConsoleManager::handlePopulateModel(Model &globalModel, const std::string &path)
{
    auto [expected, valid] =
        WeightUtils::computeExpectedWeights(
            globalModel.getArchitecture());

    if (!valid)
    {
        Logger::log(LogLevel::ERROR,
                    "[SERVER] Cannot generate weights: invalid architecture");
        std::cout << "[ERROR] Cannot generate weights: invalid architecture\n";
        return;
    }

    if (expected == 0)
    {
        Logger::log(LogLevel::ERROR,
                    "[SERVER] Expected weights is 0. Aborting.");
        std::cout << "[ERROR] Expected weights is 0. Aborting.\n";
        return;
    }

    char confirm;
    Logger::log(LogLevel::WARNING,
                "overwrite current model weights.");
    std::cout << "[WARNING] This will overwrite current model weights." << std::endl;
    std::cout << "[INFO] Expected weights count: " << expected << std::endl;
    std::cout << "[INFO] Continue? (y/n): ";

    std::cin >> confirm;

    if (confirm == 'y' || confirm == 'Y')
    {
        std::vector<float> randomWeights = WeightUtils::generateRandomWeights(expected);
        globalModel.setWeights(randomWeights);
        JSONManager::updateWeightsInJSON(path, randomWeights);

        Logger::log(LogLevel::INFO,
                    "Random weights generated and loaded into model.");
        std::cout << "[INFO] Random weights generated and loaded into model." << std::endl;
    }
    else
    {
        Logger::log(LogLevel::INFO, "Random population cancelled.");
        std::cout << "[INFO] Random population cancelled." << std::endl;
    }
}