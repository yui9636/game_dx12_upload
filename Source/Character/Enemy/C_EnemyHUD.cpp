#include "C_EnemyHUD.h"
#include "Character/Character.h" // Character�N���X�̒�`���K�v
#include "UI/UIManager.h"
#include "UI/UIProgressBar2D.h"
#include "Font/FontManager.h"
#include "Sprite/Sprite.h"
#include "Graphics.h"     // RenderContext�쐬�p
#include "Camera/Camera.h" // RenderContext�쐬�p
#include <cmath> 
#include <algorithm> 

using namespace DirectX;

C_EnemyHUD::C_EnemyHUD()
{

}

C_EnemyHUD::~C_EnemyHUD()
{
    Finalize();
}

void C_EnemyHUD::Finalize()
{
    // ���ɍ폜�ς݂Ȃ牽�����Ȃ� (reset()��nullptr�ɂȂ��Ă���͂�)
    if (grayBar) {
        UIManager::Instance().RemoveElement(grayBar);
        grayBar.reset(); // �Q�Ƃ�؂�
    }
    if (redBar) {
        UIManager::Instance().RemoveElement(redBar);
        redBar.reset();
    }
    if (whiteBar) {
        UIManager::Instance().RemoveElement(whiteBar);
        whiteBar.reset();
    }
}






void C_EnemyHUD::Start()
{
    // DX12 では Sprite が DX11 直接依存のため暫定スキップ
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) return;

    ID3D11Device* device = Graphics::Instance().GetDevice();
    barSprite = std::make_shared<Sprite>(device, "Data/Texture/UI/white0.png");


    grayBar = UIManager::Instance().CreateElement<UIProgressBar2D>();
    if (grayBar)
    {
        grayBar->SetSprite(barSprite);
        // �������̈Â��O���[
        grayBar->SetColor(0.2f, 0.2f, 0.2f, 0.5f);
        grayBar->SetSize(480.0f, 9.0f);
        grayBar->SetPosition(640.0f, 73.0f);
        grayBar->SetPivot(0.5f, 0.0f);
        // ��ɖ��^���i�w�i�Ȃ̂Łj
        grayBar->SetProgress(1.0f);
    }


    // �ԃo�[ (�w��)
    redBar = UIManager::Instance().CreateElement<UIProgressBar2D>();
    if (redBar)
    {
        redBar->SetSprite(barSprite);
        redBar->SetColor(0.8f, 0.0f, 0.0f, 1.0f);
        redBar->SetSize(480.0f, 9.0f);
        redBar->SetPosition(640.0f, 73.0f);
        redBar->SetPivot(0.5f, 0.0f);
    }

    // ���o�[ (�O��)
    whiteBar = UIManager::Instance().CreateElement<UIProgressBar2D>();
    if (whiteBar)
    {
        whiteBar->SetSprite(barSprite);
        whiteBar->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
        whiteBar->SetSize(480.0f, 9.0f);
        whiteBar->SetPosition(640.0f, 73.0f);
        whiteBar->SetPivot(0.5f, 0.0f);
        //whiteBar->SetGlow(1.0f, 1.0f, 1.0f, 1.3f);
    }
    hpPerStock = 0.0f;
}

void C_EnemyHUD::Update(float dt)
{
    if (!redBar || !whiteBar) return;

    // ���C��: Component.h �̒�`�ʂ� GetActor() ���g�p
    auto owner = GetActor();
    if (!owner) return;

    // Actor��Character�ɃL���X�g����HP���擾
    // ��Character.h ���C���N���[�h����Ă���O��
    auto character = std::dynamic_pointer_cast<Character>(owner);
    if (!character) return;

    int currentHPInt = character->GetHealth();
    int maxHPInt = character->GetMaxHealth();

    // ���S���͔�\��
    if (currentHPInt <= 0)
    {
        grayBar->SetVisible(false);
        redBar->SetVisible(false);
        whiteBar->SetVisible(false);

     
        return;
    }

    grayBar->SetVisible(true);
    redBar->SetVisible(true);
    whiteBar->SetVisible(true);

    float currentHP = static_cast<float>(currentHPInt);
    float maxHP = static_cast<float>(maxHPInt);

    // ����ݒ�
    if (hpPerStock == 0.0f && maxHP > 0)
    {
        hpPerStock = maxHP / static_cast<float>(totalStocks);
        delayedHP = currentHP;
    }

    // �_���[�W�x�����o���W�b�N
    if (currentHP < delayedHP)
    {
        if (delayTimer < DELAY_TIME) delayTimer += dt;
        else
        {
            delayedHP -= (delayedHP - currentHP) * LERP_SPEED * dt;
            if (std::abs(delayedHP - currentHP) < 0.01f)
            {
                delayedHP = currentHP;
                delayTimer = 0.0f;
            }
        }
    }
    else if (currentHP > delayedHP)
    {
        delayedHP = currentHP;
        delayTimer = 0.0f;
    }
    else
    {
        delayTimer = 0.0f;
    }

    UpdateBars(currentHP, maxHP);
}

void C_EnemyHUD::UpdateBars(float currentHP, float maxHP)
{
    // �����W�b�N�ύX: �ԃo�[(delayedHP)������X�g�b�N����ɕ\������
    // ����ɂ��A�X�g�b�N�܂����̃_���[�W�ł��u�O�̃Q�[�W�������Ă����l�q�v��������
    int visibleStock = static_cast<int>(std::ceil(delayedHP / hpPerStock));
    int currentStock = static_cast<int>(std::ceil(currentHP / hpPerStock));

    float whiteRatio = 0.0f;
    float redRatio = 0.0f;

    // ���o�[�v�Z
    if (currentStock < visibleStock)
    {
        // ���o�[�͊��ɉ��̃X�g�b�N�ɍs���Ă��܂����̂ŁA���݂̕\���X�g�b�N�ł͋�ɂ���
        whiteRatio = 0.0f;
    }
    else
    {
        // �����X�g�b�N�ɂ���Ȃ�ʏ�v�Z
        whiteRatio = std::fmod(currentHP, hpPerStock) / hpPerStock;
        if (whiteRatio == 0.0f && currentHP > 0) whiteRatio = 1.0f;
    }

    // �ԃo�[�v�Z (��ɕ\���ΏۃX�g�b�N��)
    redRatio = std::fmod(delayedHP, hpPerStock) / hpPerStock;
    if (redRatio == 0.0f && delayedHP > 0) redRatio = 1.0f;

    // �K�p
    whiteBar->SetProgress(whiteRatio);
    redBar->SetProgress(redRatio);
}

// ���C��: �����Ȃ���Render����
void C_EnemyHUD::Render()
{
    //if (!redBar || !whiteBar || !redBar->IsActive()) return;

    //auto owner = GetActor();
    //if (!owner) return;
    //auto character = std::dynamic_pointer_cast<Character>(owner);
    //if (!character) return;

    //// UI��t�H���g�`��ɕK�v�� RenderContext �������ňꎞ�쐬����
    //// Component::Render() �ɂ͈������Ȃ����߁A�V���O���g������擾���č\�z���܂�
    //RenderContext rc = {};
    //rc.commandList->GetNativeContext() = Graphics::Instance().GetDeviceContext();
    //// rc.light �����K�v�Ȃ炱���ɒǉ�

    //int currentStock = static_cast<int>(std::ceil(character->GetHealth() / hpPerStock));

    //// �{�X���̕`��
    //XMFLOAT4 nameColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    //FontManager::Instance().DrawFormat(
    //    rc.commandList->GetNativeContext(),
    //    "ComboFont",
    //    640.0f, 41.0f,
    //    nameColor,
    //    0.4f,
    //    FontAlign::Center,
    //    L"%S", bossName.c_str()
    //);

    //// �X�g�b�N���̕`��
    //float barRightX = 640.0f + (whiteBar->GetSize().x * 0.5f) + 10.0f;

    //FontManager::Instance().DrawFormat(
    //    rc.commandList->GetNativeContext(),
    //    "ComboFont",
    //    barRightX, 65.0f,
    //    nameColor,
    //    0.3f,
    //    FontAlign::Left,
    //    L"x%d", currentStock
    //);
}