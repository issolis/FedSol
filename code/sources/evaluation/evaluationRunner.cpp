#include "evaluation/evaluationRunner.h"
#include "logger/logger.h"

#include <cstdlib>
#include <string>
#include <sys/wait.h>

bool EvaluationRunner::runEvaluation(const std::string& configPath,
                                     const std::string& scriptPath)
{
    std::string command = "PYTHONPATH=code python3 -m FedSolPython.scripts." + scriptPath  + " " + configPath ;

    Logger::log(LogLevel::INFO,
                "[EVALUATOR] Running evaluation script: " + scriptPath);

    int result = system(command.c_str());

    if (result == -1)
    {
        Logger::log(LogLevel::ERROR,
                    "[EVALUATOR ERROR] Failed to execute evaluation command");
        return false;
    }

    if (WIFEXITED(result))
    {
        int exitCode = WEXITSTATUS(result);

        if (exitCode != 0)
        {
            Logger::log(LogLevel::ERROR,
                        "[EVALUATOR ERROR] Evaluation failed with code: "
                            + std::to_string(exitCode));
            return false;
        }

        Logger::log(LogLevel::INFO,
                    "[EVALUATOR] Evaluation completed successfully");
        return true;
    }

    Logger::log(LogLevel::ERROR,
                "[EVALUATOR ERROR] Evaluation process did not terminate normally");
    return false;
}