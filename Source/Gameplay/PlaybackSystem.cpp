#include "PlaybackSystem.h"
#include "PlaybackComponent.h"
#include "PlaybackRangeComponent.h"
#include "HitStopComponent.h"
#include "SpeedCurveComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include <algorithm>

static float EvaluateSpeedCurve(const SpeedCurveComponent& curve, float t01) {
    if (!curve.enabled || curve.pointCount == 0) return 1.0f;
    t01 = std::clamp(t01, 0.0f, 1.0f);
    if (t01 <= curve.points[0].t01) return curve.points[0].value;
    if (t01 >= curve.points[curve.pointCount - 1].t01) return curve.points[curve.pointCount - 1].value;
    for (int j = 0; j < curve.pointCount - 1; ++j) {
        if (t01 <= curve.points[j + 1].t01) {
            float range = curve.points[j + 1].t01 - curve.points[j].t01;
            float alpha = (range > 1e-5f) ? (t01 - curve.points[j].t01) / range : 0.0f;
            return curve.points[j].value + (curve.points[j + 1].value - curve.points[j].value) * alpha;
        }
    }
    return 1.0f;
}

void PlaybackSystem::Update(Registry& registry, float dt) {
    Signature sig = CreateSignature<PlaybackComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* playCol = arch->GetColumn(TypeManager::GetComponentTypeID<PlaybackComponent>());
        auto* hitCol = arch->GetColumn(TypeManager::GetComponentTypeID<HitStopComponent>());
        auto* curveCol = arch->GetColumn(TypeManager::GetComponentTypeID<SpeedCurveComponent>());
        auto* rangeCol = arch->GetColumn(TypeManager::GetComponentTypeID<PlaybackRangeComponent>());
        if (!playCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& pb = *static_cast<PlaybackComponent*>(playCol->Get(i));
            if (!pb.playing || pb.clipLength <= 0.0f) continue;

            float timeScale = 1.0f;

            // HitStop takes priority — override timeScale entirely
            if (hitCol) {
                auto& hs = *static_cast<HitStopComponent*>(hitCol->Get(i));
                if (hs.timer > 0.0f) {
                    timeScale = hs.speedScale;
                    hs.timer = std::max(0.0f, hs.timer - dt);
                }
            }

            // Speed curve modulation (skip if hitstop froze time)
            if (curveCol && timeScale > 0.0f) {
                auto& curve = *static_cast<SpeedCurveComponent*>(curveCol->Get(i));
                float t01;
                // Use range-space normalization if enabled and range is active
                if (curve.useRangeSpace && rangeCol) {
                    auto& range = *static_cast<PlaybackRangeComponent*>(rangeCol->Get(i));
                    float w = range.endSeconds - range.startSeconds;
                    t01 = (w > 1e-5f) ? (pb.currentSeconds - range.startSeconds) / w : 0.0f;
                } else {
                    t01 = pb.currentSeconds / pb.clipLength;
                }
                timeScale *= EvaluateSpeedCurve(curve, t01);
            }

            float delta = dt * pb.playSpeed * timeScale;
            pb.currentSeconds += delta;

            // Range-constrained playback
            if (rangeCol) {
                auto& range = *static_cast<PlaybackRangeComponent*>(rangeCol->Get(i));
                if (range.enabled) {
                    float lo = range.startSeconds;
                    float hi = range.endSeconds;
                    if (hi > lo) {
                        if (pb.currentSeconds >= hi) {
                            if (range.loopWithinRange) {
                                float w = hi - lo;
                                pb.currentSeconds = lo + fmodf(pb.currentSeconds - lo, w);
                            } else {
                                pb.currentSeconds = hi;
                                if (pb.stopAtEnd) { pb.playing = false; pb.finished = true; }
                            }
                        } else if (pb.currentSeconds < lo) {
                            pb.currentSeconds = lo;
                        }
                        continue; // Skip full-clip logic
                    }
                }
            }

            // Full-clip end / loop
            if (pb.currentSeconds >= pb.clipLength) {
                if (pb.looping) {
                    pb.currentSeconds = fmodf(pb.currentSeconds, pb.clipLength);
                } else {
                    pb.currentSeconds = pb.clipLength;
                    if (pb.stopAtEnd) { pb.playing = false; pb.finished = true; }
                }
            } else if (pb.currentSeconds < 0.0f) {
                if (pb.looping) {
                    pb.currentSeconds = pb.clipLength + fmodf(pb.currentSeconds, pb.clipLength);
                } else {
                    pb.currentSeconds = 0.0f;
                }
            }
        }
    }
}
