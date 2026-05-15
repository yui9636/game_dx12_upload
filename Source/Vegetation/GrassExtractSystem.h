#pragma once

class Registry;
class RenderQueue;
class GrassBuildSystem;

class GrassExtractSystem {
public:
    void Extract(Registry& registry, const GrassBuildSystem& buildSys, RenderQueue& queue);
};
