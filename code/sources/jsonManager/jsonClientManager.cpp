#include "jsonManager/jsonClientManager.h"

using json = nlohmann::json;

void JSONClientManager::startClientFromJSON(const std::string &path)
{
    json j = JSONManager::load(path);

    // -------- CLIENT --------
    uint32_t id = j["client"]["id"];

    // -------- CONNECTION --------
    std::string serverIP = j["connection"]["server_ip"];
    int port = j["connection"]["port"];
    std::string password = j["connection"]["password"];

    std::string datasetPath = j["dataset"].contains("receive_path")
        ? j["dataset"]["receive_path"].get<std::string>()
        : j["dataset"]["train_path"].get<std::string>();

    // ── pt_path mode ─────────────────────────────────────────────────────────
    if (j["model"].contains("pt_path"))
    {
        std::string ptPath = j["model"]["pt_path"];
        std::cout << "[INFO] pt_path mode detected: " << ptPath << "\n";
        std::cout << "[INFO] Extracting weights from pretrained model...\n";

        // 1. Run Python script: infers architecture ONLY (no weights) into JSON
        std::string cmd =
            "PYTHONPATH=code python3 -m FedSolPython.scripts.load_pt_arch \"" + path + "\"";

        int ret = system(cmd.c_str());
        int exitCode = WEXITSTATUS(ret);

        if (ret == -1 || exitCode != 0)
        {
            throw std::runtime_error(
                "[JSONClientManager] load_pt_weights failed (exit " +
                std::to_string(exitCode) + ") for: " + path);
        }

        // 2. Compute architecture hash (stable across rounds — weights are excluded)
        std::string hashCmd = "PYTHONPATH=code python3 -m FedSolPython.scripts.get_pt_arch_hash \"" + ptPath + "\"";
        FILE *pipe = popen(hashCmd.c_str(), "r");
        if (!pipe)
            throw std::runtime_error("[JSONClientManager] Failed to compute .pt arch hash");

        char hashBuf[65] = {};
        if (fgets(hashBuf, sizeof(hashBuf), pipe) == nullptr)
        {
            pclose(pipe);
            throw std::runtime_error("[JSONClientManager] Failed to read .pt arch hash");
        }
        pclose(pipe);

        std::string ptHash(hashBuf);
        if (!ptHash.empty() && ptHash.back() == '\n')
            ptHash.pop_back();

        std::cout << "[INFO] .pt arch hash: " << ptHash.substr(0, 8) << "...\n";

        // 3. Reload JSON (now has architecture + weights)
        j = JSONManager::load(path);

        // 4. Build model from JSON (architecture needed for weight storage in C++)
        std::vector<float> weights;
        if (j["model"].contains("weights"))
            weights = j["model"]["weights"].get<std::vector<float>>();

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

        // 5. Start client in pt_path mode — handshake uses hash, not architecture
        Client client(port, serverIP, password, model, path, datasetPath, ptHash);
        client.run();
        return;
    }

    // ── Normal mode ───────────────────────────────────────────────────────────

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
    Client client(port, serverIP, password, model, path, datasetPath);
    client.run();
}