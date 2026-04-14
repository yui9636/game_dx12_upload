#pragma once

#include <cstddef>
#include <string>

struct ModelSerializerSettings
{
    float scaling = 1.0f;
    bool enableSimplification = true;
    float targetTriangleRatio = 0.35f;
    float targetError = 0.02f;
    bool lockBorder = true;
    bool optimizeVertexCache = true;
    bool optimizeOverdraw = true;
    float overdrawThreshold = 1.05f;
    bool optimizeVertexFetch = true;
};

struct ModelSerializerResult
{
    bool success = false;
    std::string sourcePath;
    std::string outputPath;
    std::string message;
    size_t processedMeshCount = 0;
    size_t simplifiedMeshCount = 0;
    size_t skippedSimplificationMeshCount = 0;
    size_t sourceVertexCount = 0;
    size_t sourceIndexCount = 0;
    size_t outputVertexCount = 0;
    size_t outputIndexCount = 0;
};

class ModelAssetSerializer
{
public:
    static ModelSerializerResult Build(
        const std::string& sourcePath,
        const std::string& outputPath,
        const ModelSerializerSettings& settings = {});
};
