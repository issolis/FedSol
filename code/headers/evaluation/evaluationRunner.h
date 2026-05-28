#ifndef EVALUATIONRUNNER_H
#define EVALUATIONRUNNER_H

#include <string>

class EvaluationRunner
{
public:
    static bool runEvaluation(const std::string& configPath,
                              const std::string& scriptPath);
};

#endif