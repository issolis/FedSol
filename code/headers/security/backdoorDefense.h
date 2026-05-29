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
        size_t clientsBefore = 0;
        size_t clientsAfter  = 0;
        std::vector<uint32_t> acceptedIDs;
        std::vector<uint32_t> rejectedIDs;
        bool applied = false;      
        bool skippedNoHistory = false; 
    };
    static constexpr float COSINE_THRESHOLD = 0.0f;
    static Result filter(
        bool enabled,
        const std::vector<float> &globalWeightsPrev,
        const std::vector<float> &globalWeightsPrevPrev,
        std::vector<std::vector<float>> &weightsList,
        std::vector<uint32_t> &sampleSizesList,
        std::vector<uint32_t> &clientIDs);
private:
    static double dot(const std::vector<float> &a, const std::vector<float> &b);
    static double norm(const std::vector<float> &v);
    static float  median(std::vector<float> values);
    static std::vector<float> subtract(const std::vector<float> &a,
                                        const std::vector<float> &b);
};
#endif