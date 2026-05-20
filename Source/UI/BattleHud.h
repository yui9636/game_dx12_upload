// BattleHud: on-screen presentation for a 1v1 battle.
// Reads BattleFlowSystem phase / remaining time and draws a centered banner
// ("FIGHT!", "YOU WIN", "YOU LOSE", "DRAW") plus a top-center countdown timer.
#pragma once

struct RenderContext;

class BattleHud {
public:
    static BattleHud& Instance()
    {
        static BattleHud instance;
        return instance;
    }

    // Advances the banner animation and detects BattleFlow phase changes.
    void Update(float dt);

    // Draws the timer and banner. Call inside the HUD sprite batch.
    void Render(const RenderContext& rc);

    // Clears banner/phase state (call when a battle is reset).
    void Reset();

private:
    BattleHud() = default;
    ~BattleHud() = default;

    int   m_lastPhase     = -1;       // last seen BattleFlow phase
    float m_bannerTimer   = 0.0f;     // remaining banner display time
    float m_bannerDuration = 0.0f;    // total banner display time
    const wchar_t* m_bannerText = nullptr;
};
