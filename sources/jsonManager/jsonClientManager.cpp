#include "jsonManager/jsonClientManager.h"

using json = nlohmann::json;

void JSONClientManager::startClientFromJSON(const std::string &path)
{
    json j = JSONManager::load(path);
    std::string path = j["path"]; 

    // -------- CLIENT --------
    uint32_t id = j["client"]["id"];

    // -------- CONNECTION  --------
    std::string serverIP = j["connection"]["server_ip"];
    int port = j["connection"]["port"];
    std::string password = j["connection"]["password"];

    // -------- WEIGHTS --------
    std::vector<float> weights;

    if (j["model"].contains("weights"))
        weights = j["model"]["weights"].get<std::vector<float>>();

    // -------- ARCHITECTURE --------
    Architecture arch;

    for (const auto &l : j["model"]["architecture"]["layers"])
    {
        Layer layer;

        layer.type = static_cast<LayerType>(l["type"].get<int>());

        if (l.contains("output_dim"))
        {
            auto dims = l["output_dim"];
            for (int i = 0; i < 3; i++)
                layer.output_dim[i] = dims[i];
        }

        if (l.contains("input_dim"))
        {
            auto dims = l["input_dim"];
            for (int i = 0; i < 3; i++)
                layer.input_dim[i] = dims[i];
        }

        if (l.contains("in_features"))
        {
            layer.in_features = l["in_features"];
            layer.out_features = l["out_features"];
        }

        if (l.contains("kernel_size"))
        {
            layer.kernel_size = l["kernel_size"];
            layer.stride = l["stride"];
            layer.padding = l["padding"];
        }

        if (l.contains("activation"))
        {
            layer.activation = static_cast<ActivationType>(l["activation"].get<int>());
        }

        arch.addLayer(layer);
    }

    // -------- MODEL --------
    Model model(id, arch, weights);

    // -------- CLIENT --------
    Client client(port, serverIP, password, model, path);
    client.run(); 
}