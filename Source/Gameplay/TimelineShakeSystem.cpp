#include "TimelineShakeSystem.h"
#include "TimelineComponent.h"
#include "TimelineItemBuffer.h"
#include "HitStopComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include <cmath>
#include <algorithm>

DirectX::XMFLOAT3 TimelineShakeSystem::s_shakeOffset = { 0, 0, 0 };
float TimelineShakeSystem::s_shakeTime = 0.0f;

DirectX::XMFLOAT3 TimelineShakeSystem::GetShakeOffset() { return s_shakeOffset; }
void TimelineShakeSystem::ResetShakeOffset() { s_shakeOffset = { 0, 0, 0 }; }

void TimelineShakeSystem::Update(Registry& registry, float dt) {
    s_shakeOffset = { 0, 0, 0 };
    s_shakeTime += dt;

    Signature sig = CreateSignature<TimelineComponent, TimelineItemBuffer>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* tlCol  = arch->GetColumn(TypeManager::GetComponentTypeID<TimelineComponent>());
        auto* bufCol = arch->GetColumn(TypeManager::GetComponentTypeID<TimelineItemBuffer>());
        auto* hsCol  = arch->GetColumn(TypeManager::GetComponentTypeID<HitStopComponent>());
        if (!tlCol || !bufCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& tl  = *static_cast<TimelineComponent*>(tlCol->Get(i));
            auto& buf = *static_cast<TimelineItemBuffer*>(bufCol->Get(i));

            for (auto& item : buf.items) {
                if (item.type != 4) continue;
                bool inside = tl.currentFrame >= item.start && tl.currentFrame <= item.end;

                // Reset fire flag when leaving or before entering
                if (tl.currentFrame < item.start) {
                    item.fired = false;
                }

                // Fire hitstop on enter (once)
                if (inside && !item.fired) {
                    item.fired = true;
                    if (item.shake.hitStopDuration > 0.0f && hsCol) {
                        auto& hs = *static_cast<HitStopComponent*>(hsCol->Get(i));
                        hs.timer = item.shake.hitStopDuration;
                        hs.speedScale = item.shake.timeScale;
                    }
                }

                // Compute shake offset while inside
                if (inside && tl.fps > 0.0f) {
                    float elapsed = static_cast<float>(tl.currentFrame - item.start) / tl.fps;
                    float duration = item.shake.duration;
                    if (duration <= 0.0f) duration = 0.2f;

                    float progress = std::clamp(elapsed / duration, 0.0f, 1.0f);
                    float decay = 1.0f - progress;
                    float amp = item.shake.amplitude * decay;

                    if (amp > 0.001f) {
                        float t = s_shakeTime * item.shake.frequency;
                        // Procedural noise — layered sin/cos for organic feel
                        float nx = sinf(t) + sinf(t * 0.5f + 1.5f) * 0.5f;
                        float ny = cosf(t * 1.1f) + sinf(t * 0.8f + 2.0f) * 0.5f;
                        float nz = sinf(t * 0.3f) * 0.3f;

                        s_shakeOffset.x += nx * amp;
                        s_shakeOffset.y += ny * amp;
                        s_shakeOffset.z += nz * amp;
                    }
                }
            }
        }
    }
}
