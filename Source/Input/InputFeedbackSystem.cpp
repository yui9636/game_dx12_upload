#include "InputFeedbackSystem.h"
#include "VibrationRequestComponent.h"
#include "InputUserComponent.h"
#include "IInputBackend.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include <unordered_map>

void InputFeedbackSystem::Update(Registry& registry, IInputBackend& backend, float dt) {
    // user ごとに vibration request を集約する。
    struct MergedVibration {
        float left = 0.0f;
        float right = 0.0f;
    };
    std::unordered_map<uint8_t, MergedVibration> merged;

    Signature vibSig = CreateSignature<VibrationRequestComponent>();
    auto archetypes = registry.GetAllArchetypes();

    // VibrationRequestComponent を外す entity を記録する。
    std::vector<std::pair<Archetype*, size_t>> expired;

    for (auto* arch : archetypes) {
        if (!SignatureMatches(arch->GetSignature(), vibSig)) continue;
        auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<VibrationRequestComponent>());
        if (!col) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& vib = *static_cast<VibrationRequestComponent*>(col->Get(i));
            auto& m = merged[vib.targetUserId];
            if (vib.leftMotor > m.left) m.left = vib.leftMotor;
            if (vib.rightMotor > m.right) m.right = vib.rightMotor;

            vib.duration -= dt;
            if (vib.duration <= 0.0f) {
                expired.push_back({ arch, i });
            }
        }
    }

    // user に対応する device ID を探して vibration を適用する。
    Signature userSig = CreateSignature<InputUserComponent>();
    for (auto* arch : archetypes) {
        if (!SignatureMatches(arch->GetSignature(), userSig)) continue;
        auto* userCol = arch->GetColumn(TypeManager::GetComponentTypeID<InputUserComponent>());
        if (!userCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& user = *static_cast<InputUserComponent*>(userCol->Get(i));
            auto it = merged.find(user.userId);
            if (it != merged.end()) {
                // deviceMask を簡易的な device ID として扱う。
                backend.SetVibration(user.deviceMask, it->second.left, it->second.right);
            }
        }
    }

    // active request がない user の vibration を停止する。
    // ここでは簡略化し、SDL duration による自動停止に任せる。
}
