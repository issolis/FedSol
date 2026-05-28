#ifndef MODEL_EXPORTER_H
#define MODEL_EXPORTER_H

#include <string>

class ModelExporter {
public:
    static bool exportModel(const std::string& configPath,
                            const std::string& scriptPath = "build_and_save");
};

#endif