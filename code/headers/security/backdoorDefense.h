#ifndef BACKDOORDEFENSE_H
#define BACKDOORDEFENSE_H

#include <vector>
#include <cstdint>
#include <string>



class BackdoorDefense
{
public:
    struct Result
    {
        size_t clientsBefore = 0;          // cohort size received by the server
        size_t clientsAfter  = 0;          // cohort size kept after filtering
        std::vector<uint32_t> acceptedIDs; // client IDs kept
        std::vector<uint32_t> rejectedIDs; // client IDs flagged as poisoned
        bool applied = false;              // false if the defense was disabled
    };

    static constexpr float COSINE_OUTLIER_THRESHOLD = 0.0f;

    static Result filter(
        bool enabled,
        const std::vector<float> &previousGlobalWeights,
        std::vector<std::vector<float>> &weightsList,
        std::vector<uint32_t> &sampleSizesList,
        std::vector<uint32_t> &clientIDs);

private:
    static double dot(const std::vector<float> &a, const std::vector<float> &b);
    static double norm(const std::vector<float> &a);
    static float median(std::vector<float> values);
};

#endif