#include "datasetutils/datasetutils.h"

#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

uint32_t DatasetUtils::getTotalSamplesFromNPZ(const std::string &datasetPath,
                                              const std::string &scriptPath)
{
    std::string command = "PYTHONPATH=code python3 -m FedSolPython.scripts." + scriptPath+ " " + datasetPath;

    std::array<char, 128> buffer{};
    std::string output;

    FILE *pipe = popen(command.c_str(), "r");
    if (pipe == nullptr)
    {
        throw std::runtime_error("[DatasetUtils] Failed to open pipe to Python script");
    }

    try
    {
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
        {
            output += buffer.data();
        }
    }
    catch (...)
    {
        pclose(pipe);
        throw;
    }

    int status = pclose(pipe);
    if (status != 0)
    {
        throw std::runtime_error("[DatasetUtils] Python script failed while reading total samples");
    }

    try
    {
        return static_cast<uint32_t>(std::stoul(output));
    }
    catch (...)
    {
        throw std::runtime_error("[DatasetUtils] Invalid output received from Python script: " + output);
    }
}