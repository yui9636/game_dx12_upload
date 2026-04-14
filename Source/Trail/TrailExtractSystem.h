#pragma once

class Registry;
class RenderQueue;
struct RenderContext;

class TrailExtractSystem
{
public:
    static void Extract(Registry& registry, RenderQueue& queue, const RenderContext& rc);
};
