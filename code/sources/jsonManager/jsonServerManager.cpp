#include "jsonManager/jsonServerManager.h"
#include <cstdlib>
#include <iostream>

using json = nlohmann::json;

void JSONServerManager::startServerFromJSON(const std::string &path)
{
    json j = JSONManager::load(path);

    // --- SERVER ---
    unsigned short port = j["port"];
    uint32_t backlog = j["backlog"];

    // --- MODEL ---
    uint32_t id = j["model"]["id"];

    // --- DATASET PATH ---
    std::string datasetPath = j["dataset"]["train_path"];

    // --- FedSol V2: backdoor defense toggle ---
    // Optional top-level boolean. Defaults to false so existing configs that
    // predate the defense keep the original FedAvg behavior.
    bool defenseEnabled = false;
    if (j.contains("defense"))
        defenseEnabled = j["defense"].get<bool>();
    std::cout << "[INFO] Backdoor defense: "
              << (defenseEnabled ? "ENABLED" : "DISABLED") << "\n";

    // ── pt_path mode ─────────────────────────────────────────────────────────
    if (j["model"].contains("pt_path"))
    {
        std::string ptPath = j["model"]["pt_path"];
        std::cout << "[INFO] Server pt_path mode detected: " << ptPath << "\n";
        std::cout << "[INFO] Extracting weights from pretrained model...\n";

        // 1. Run Python script: infers architecture + extracts weights into JSON
        std::string cmd =
            "PYTHONPATH=code python3 -m FedSolPython.scripts.load_pt_weights \"" + path + "\"";

        int ret = system(cmd.c_str());
        int exitCode = WEXITSTATUS(ret);

        if (ret == -1 || exitCode != 0)
        {
            throw std::runtime_error(
                "[JSONServerManager] load_pt_weights failed (exit " +
                std::to_string(exitCode) + ") for: " + path);
        }

        // 2. Compute architecture hash (stable across rounds — weights are excluded)
        std::string hashCmd = "PYTHONPATH=code python3 -m FedSolPython.scripts.get_pt_arch_hash \"" + ptPath + "\"";
        FILE *pipe = popen(hashCmd.c_str(), "r");
        if (!pipe)
            throw std::runtime_error("[JSONServerManager] Failed to compute .pt arch hash");

        char hashBuf[65] = {};
        if (fgets(hashBuf, sizeof(hashBuf), pipe) == nullptr)
        {
            pclose(pipe);
            throw std::runtime_error("[JSONServerManager] Failed to read .pt arch hash");
        }
        pclose(pipe);

        std::string ptHash(hashBuf);
        if (!ptHash.empty() && ptHash.back() == '\n')
            ptHash.pop_back();

        std::cout << "[INFO] .pt arch hash: " << ptHash.substr(0, 8) << "...\n";

        // 3. Reload JSON (now has architecture + weights)
        j = JSONManager::load(path);

        std::vector<float> weights = j["model"]["weights"].get<std::vector<float>>();

        Architecture arch;
        for (const auto &l : j["model"]["architecture"]["layers"])
        {
            Layer layer;
            layer.type = static_cast<LayerType>(l["type"].get<int>());
            if (l.contains("output_dim")) { auto d = l["output_dim"]; for (int i = 0; i < 3; i++) layer.output_dim[i] = d[i]; }
            if (l.contains("input_dim"))  { auto d = l["input_dim"];  for (int i = 0; i < 3; i++) layer.input_dim[i] = d[i]; }
            if (l.contains("in_features")) { layer.in_features = l["in_features"]; layer.out_features = l["out_features"]; }
            if (l.contains("kernel_size")) { layer.kernel_size = l["kernel_size"]; layer.stride = l["stride"]; layer.padding = l["padding"]; }
            if (l.contains("activation"))  { layer.activation = static_cast<ActivationType>(l["activation"].get<int>()); }
            arch.addLayer(layer);
        }

        Model model(id, arch, weights);

        Server server(port, backlog, model, path, datasetPath, defenseEnabled, ptHash);
        server.run();
        return;
    }

    // ── Normal mode ───────────────────────────────────────────────────────────

    std::vector<float> weights = j["model"]["weights"].get<std::vector<float>>();

    // --- ARCHITECTURE ---
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

    // --- MODEL FINAL ---
    Model model(id, arch, weights);

    // --- SERVER FINAL ---
    Server server(port, backlog, model, path, datasetPath, defenseEnabled);
    server.run();
}