#include "C_PlayerHUD.h"

#include "Character/Character.h"
#include "UI/UIManager.h"
#include "UI/UIProgressBar3D.h"
#include "Sprite/Sprite3D.h"
#include "Camera/Camera.h"
#include "Graphics.h"
#include "Player.h"
#include "UI/UIScreen.h"
#include "Sprite/Sprite.h"

using namespace DirectX;

void C_PlayerHUD::Start()
{
   


    hpBar = UIManager::Instance().CreateElement<UIProgressBar3D>();

    hpNumber = UIManager::Instance().CreateElement<UIHPNumber>();

    comboUI = UIManager::Instance().CreateElement<UICombo>();

    attackIcon = UIManager::Instance().CreateElement<UIScreen>();

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) return;

    ID3D11Device* device = Graphics::Instance().GetDevice();
    barSprite = std::make_shared<Sprite3D>(device, "Data/Texture/UI/white.png");

    //attackSprite = std::make_shared<Sprite>(device, "Data/Texture/UI/AttackIcon.png");

    if (attackIcon)
    {
        attackIcon->SetSprite(attackSprite);

        attackIcon->SetPosition(1090.0f, 590.0f);

        attackIcon->SetSize(168.0f, 168.0f);

        attackIcon->SetPivot(0.5f, 0.5f);

   
         attackIcon->SetGlow(1.0f, 1.0f, 1.0f, 1.3f);
    }


    if (hpBar)
    {
        hpBar->SetSprite(barSprite);

        hpBar->SetSize(1.0f, 0.04f);

        hpBar->SetRotation(0.0f, -28.0f, 0.0f);

    }

    if (hpNumber)
    {
        hpNumber->SetRotation(0.0f, -30.0f, 0.0f);
    }

}

void C_PlayerHUD::Update(float dt)
{
    if (!hpBar) return;

    // --------------------------------------------------------
    // --------------------------------------------------------
    Camera& cam = Camera::Instance();
    XMFLOAT3 eye = cam.GetEye();
    XMFLOAT3 front = cam.GetFront();
    XMFLOAT3 up = cam.GetUp();
    XMFLOAT3 right = cam.GetRight();

    XMVECTOR vEye = XMLoadFloat3(&eye);
    XMVECTOR vFront = XMLoadFloat3(&front);
    XMVECTOR vUp = XMLoadFloat3(&up);
    XMVECTOR vRight = XMLoadFloat3(&right);

    float dist = 3.0f;
    float offsetX = -1.4f;
    float offsetY = 1.0f;

    XMVECTOR vPos = vEye + (vFront * dist) + (vRight * offsetX) + (vUp * offsetY);

    XMFLOAT3 finalPos;
    XMStoreFloat3(&finalPos, vPos);

    hpBar->SetPosition(finalPos);

    if (hpNumber)
    {
        float numShiftX = -0.55f;
        float numShiftY = 0.13f;

        XMVECTOR vNumPos = vPos + (vRight * numShiftX) + (vUp * numShiftY);

        XMFLOAT3 finalNumPos;
        XMStoreFloat3(&finalNumPos, vNumPos);

        hpNumber->SetPosition(finalNumPos);

    }

    // --------------------------------------------------------
    // --------------------------------------------------------
    auto actor = GetActor();
    if (actor)
    {
        auto player = std::dynamic_pointer_cast<Player>(actor);
        if (player)
        {
            int currentHp = player->GetHealth();
            int maxHp = player->GetMaxHealth();

            float ratio = 1.0f;
            if (maxHp > 0)
            {
                ratio = static_cast<float>(currentHp) / static_cast<float>(maxHp);
            }
            hpBar->SetProgress(ratio);

        if (hpNumber)
        {
            hpNumber->SetHP(currentHp, maxHp);
        }

        }

      

        //if (player && comboUI)
        //{
        //    comboUI->SetCombo(player->GetComboCount());
        //}

    }
}
