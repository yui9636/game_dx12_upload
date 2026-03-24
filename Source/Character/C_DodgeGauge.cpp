#include "C_DodgeGauge.h"
#include "UI/UIScreen.h"
#include "UI/UIManager.h"
#include "Sprite/Sprite.h"
#include "Graphics.h"
#include <imgui.h>
#include <cmath>

using namespace DirectX;

C_DodgeGauge::C_DodgeGauge()
{
    uiPosition = { 906.0f, 583.0f};
    iconSize = { 426.0f, 251.0f };
    gaugeSize = { 111.0f, 104.0f };
    gaugePivot = { 0.0f, 0.936f };

    gaugeOffset = { -52.0f, 48.0f };

    gap = 46.500f;
}

C_DodgeGauge::~C_DodgeGauge()
{
    auto& ui = UIManager::Instance();
    if (centerIcon) ui.RemoveElement(centerIcon);
    for (int i = 0; i < 4; ++i)
    {
        if (segments[i]) ui.RemoveElement(segments[i]);
    }
}

void C_DodgeGauge::Start()
{
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) return;

    ID3D11Device* device = Graphics::Instance().GetDevice();
    auto& ui = UIManager::Instance();

    iconSprite = std::make_shared<Sprite>(device, "Data/Texture/UI/DodgeIcon.png");
    gaugeSprite = std::make_shared<Sprite>(device, "Data/Texture/UI/gauge.png");

    centerIcon = ui.CreateElement<UIScreen>();
    centerIcon->SetSprite(iconSprite);
    centerIcon->SetPivot(0.5f, 0.5f);

    for (int i = 0; i < 4; ++i)
    {
        segments[i] = ui.CreateElement<UIScreen>();
        segments[i]->SetSprite(gaugeSprite);

        segments[i]->SetRotation(i * 90.0f);

        segments[i]->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
    }


    if (centerIcon)
    {
        centerIcon->SetGlow(1.0f, 1.0f, 1.0f, 1.3f);
    }

    for (int i = 0; i < 4; ++i)
    {
        if (segments[i])
        {
            segments[i]->SetGlow(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }



    UpdateLayout();
}

void C_DodgeGauge::Update(float dt)
{
    if (currentStamina < MAX_STAMINA)
    {
        if (recoveryTimer > 0.0f) recoveryTimer -= dt;
        else
        {
            currentStamina += RECOVERY_RATE * dt;
            if (currentStamina > MAX_STAMINA) currentStamina = MAX_STAMINA;
        }
    }

    UpdateLayout();
    UpdateState();
}

void C_DodgeGauge::UpdateLayout()
{
    if (centerIcon)
    {
        centerIcon->SetPosition(uiPosition.x, uiPosition.y);
        centerIcon->SetSize(iconSize.x, iconSize.y);
    }

    XMFLOAT2 offsets[4] = {
        { +gap, -gap },
        { +gap, +gap },
        { -gap, +gap },
        { -gap, -gap }
    };

    for (int i = 0; i < 4; ++i)
    {
        if (segments[i])
        {
            segments[i]->SetSize(gaugeSize.x, gaugeSize.y);
            segments[i]->SetPivot(gaugePivot.x, gaugePivot.y);

            float px = uiPosition.x + offsets[i].x + gaugeOffset.x;
            float py = uiPosition.y + offsets[i].y + gaugeOffset.y;

            segments[i]->SetPosition(px, py);
        }
    }
}

void C_DodgeGauge::UpdateState()
{
    int currentStock = static_cast<int>(currentStamina / COST_PER_DODGE);

    for (int i = 0; i < 4; ++i)
    {
        if (!segments[i]) continue;
        segments[i]->SetVisible(currentStock > i);
    }
    if (centerIcon) centerIcon->SetVisible(true);
}

bool C_DodgeGauge::TryConsumeStamina()
{
    if (currentStamina >= COST_PER_DODGE)
    {
        currentStamina -= COST_PER_DODGE;
        recoveryTimer = RECOVERY_DELAY;
        return true;
    }
    return false;
}

void C_DodgeGauge::OnGUI()
{
    if (ImGui::CollapsingHeader("Dodge Gauge Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Global");
        ImGui::DragFloat2("UI Position", &uiPosition.x, 1.0f);

        ImGui::Separator();
        ImGui::Text("Icon");
        ImGui::DragFloat2("Icon Size", &iconSize.x, 1.0f);

        ImGui::Separator();
        ImGui::Text("Gauge");
        ImGui::DragFloat2("Gauge Size", &gaugeSize.x, 1.0f);
        ImGui::DragFloat2("Gauge Pivot", &gaugePivot.x, 0.001f, -2.0f, 2.0f);

        ImGui::DragFloat2("Gauge Offset", &gaugeOffset.x, 1.0f);

        ImGui::DragFloat("Gap (Spread)", &gap, 0.5f);

        ImGui::Separator();
        ImGui::Text("Stamina: %.0f / %.0f (Stock: %d)", currentStamina, MAX_STAMINA, (int)(currentStamina / COST_PER_DODGE));
        if (ImGui::Button("Consume")) TryConsumeStamina();
        if (ImGui::Button("Fill")) currentStamina = MAX_STAMINA;
    }
}
