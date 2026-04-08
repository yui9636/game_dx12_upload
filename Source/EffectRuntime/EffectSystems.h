#pragma once

class Registry;
struct RenderContext;
class RenderQueue;

class EffectSpawnSystem
{
public:
    static void Update(Registry& registry, float dt);
};

class EffectPlaybackSystem
{
public:
    static void Update(Registry& registry, float dt);
};

class EffectAttachmentSystem
{
public:
    static void Update(Registry& registry);
};

class EffectSimulationSystem
{
public:
    static void Update(Registry& registry, float dt);
};

class EffectLifetimeSystem
{
public:
    static void Update(Registry& registry, float dt);
};

class EffectPreviewSystem
{
public:
    static void Update(Registry& registry, float dt);
};

class EffectExtractSystem
{
public:
    static void Extract(Registry& registry, RenderContext& rc, RenderQueue& queue);
};
