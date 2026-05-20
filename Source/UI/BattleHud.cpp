#include "BattleHud.h"

#include "Gameplay/BattleFlowSystem.h"
#include "Gameplay/BattleFlowComponent.h"
#include "RenderContext/RenderContext.h"
#include "Font/FontManager.h"

#include <algorithm>
#include <cmath>

namespace {
    // HUD-shared font key (also used by combo / damage popups).
    constexpr const char* kBattleFont = "ComboFont";
    constexpr float kBannerDuration = 2.2f;
}

void BattleHud::Reset()
{
    m_lastPhase = -1;
    m_bannerTimer = 0.0f;
    m_bannerDuration = 0.0f;
    m_bannerText = nullptr;
}

void BattleHud::Update(float dt)
{
    using Phase = BattleFlowComponent::Phase;
    const int phase = BattleFlowSystem::GetPhase();

    if (phase != m_lastPhase) {
        const wchar_t* text = nullptr;
        switch (static_cast<Phase>(phase)) {
        case Phase::Combat:  text = L"FIGHT!";   break;
        case Phase::Victory: text = L"YOU WIN";  break;
        case Phase::Defeat:  text = L"YOU LOSE"; break;
        case Phase::Draw:    text = L"DRAW";     break;
        default: break;
        }
        if (text) {
            m_bannerText = text;
            m_bannerDuration = kBannerDuration;
            m_bannerTimer = kBannerDuration;
        }
        m_lastPhase = phase;
    }

    if (m_bannerTimer > 0.0f) {
        m_bannerTimer = (std::max)(0.0f, m_bannerTimer - dt);
    }
}

void BattleHud::Render(const RenderContext& rc)
{
    if (!rc.commandList) {
        return;
    }
    const float w = static_cast<float>(rc.displayWidth);
    const float h = static_cast<float>(rc.displayHeight);
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }

    // --- Countdown timer (top-center) ---
    const float remaining = BattleFlowSystem::GetRemainingTime();
    if (remaining >= 0.0f) {
        const int seconds = static_cast<int>(std::ceil(remaining));
        const DirectX::XMFLOAT4 timerColor = (seconds <= 10)
            ? DirectX::XMFLOAT4{ 1.0f, 0.28f, 0.22f, 1.0f }
            : DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
        FontManager::Instance().DrawFormat(
            rc.commandList, kBattleFont,
            w * 0.5f, h * 0.045f,
            timerColor, 1.7f, FontAlign::Center,
            L"%d", seconds);
    }

    // --- Centered banner (FIGHT! / YOU WIN / ...) ---
    if (m_bannerTimer > 0.0f && m_bannerText) {
        const float elapsed = m_bannerDuration - m_bannerTimer;
        const float fadeIn = 0.25f;
        const float fadeOut = 0.6f;

        float alpha = 1.0f;
        if (elapsed < fadeIn) {
            alpha = elapsed / fadeIn;
        }
        else if (m_bannerTimer < fadeOut) {
            alpha = m_bannerTimer / fadeOut;
        }
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        // Slight scale "punch" as it appears.
        float scale = 3.0f;
        if (elapsed < fadeIn) {
            scale += (1.0f - elapsed / fadeIn) * 1.4f;
        }

        const DirectX::XMFLOAT4 bannerColor{ 1.0f, 0.93f, 0.38f, alpha };
        FontManager::Instance().DrawFormat(
            rc.commandList, kBattleFont,
            w * 0.5f, h * 0.42f,
            bannerColor, scale, FontAlign::Center,
            L"%s", m_bannerText);
    }
}
