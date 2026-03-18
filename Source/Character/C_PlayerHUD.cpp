#include "C_PlayerHUD.h"

// �K�v�ȃw�b�_
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
   


    // 1. 3D HP�o�[����
    hpBar = UIManager::Instance().CreateElement<UIProgressBar3D>();

    hpNumber = UIManager::Instance().CreateElement<UIHPNumber>();

    comboUI = UIManager::Instance().CreateElement<UICombo>();

    attackIcon = UIManager::Instance().CreateElement<UIScreen>();

    // 2. 3D�X�v���C�g����
    // ���m���ɓǂݍ��߂�p�X���w�肵�Ă��������B�ǂݍ��ݎ��s���̓_�~�[(1x1��)�ɂȂ�܂��B
    // DX12 では Sprite3D が DX11 直接依存のため暫定スキップ
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) return;

    ID3D11Device* device = Graphics::Instance().GetDevice();
    barSprite = std::make_shared<Sprite3D>(device, "Data/Texture/UI/white.png");

    //attackSprite = std::make_shared<Sprite>(device, "Data/Texture/UI/AttackIcon.png");

    if (attackIcon)
    {
        attackIcon->SetSprite(attackSprite);

        // --- ���W�ݒ� ---
        // ����Q�[�W(906, 583)�̍��ׂ�����ɔz�u
        attackIcon->SetPosition(1090.0f, 590.0f);

        // --- �T�C�Y�ݒ� ---
        // �����傫�߂̃{�^���Ƃ��Ĕz�u (128x128���x)
        // ���摜�̃T�C�Y���g�������ꍇ�� attackSprite->GetTextureWidth() �����g�p
        attackIcon->SetSize(168.0f, 168.0f);

        // --- �s�{�b�g�ݒ� ---
        // ���S�
        attackIcon->SetPivot(0.5f, 0.5f);

   
        // �ڂ��蔭�����������ꍇ�͈ȉ���ǉ�
         attackIcon->SetGlow(1.0f, 1.0f, 1.0f, 1.3f);
    }


    if (hpBar)
    {
        // �摜�Z�b�g (�w�i�͐ݒ肵�Ȃ�)
        hpBar->SetSprite(barSprite);

        // --- �`��ݒ� ---
        // ���[�g���P�ʁB�\������Ȃ��ꍇ�͂܂��傫�߂ɂ��Ċm�F����
        // ��3m, ����0.15m
        hpBar->SetSize(1.0f, 0.04f);

        // --- ��]�ݒ� ---
        // Y���ɉ�]�����āu�΂߁v�Ɍ�����
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
    // A. �J�����Ǐ]���� (3D HUD��)
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

    // �����W�v�Z: �J�����́u�ڂ̑O�v�ɔz�u
    // dist: �J��������̋��� (�߂�����ƃN���b�v����A��������Ɩ������B3.0f�����肪���S)
    float dist = 3.0f;
    float offsetX = -1.4f; // ����
    float offsetY = 1.0f;  // ���

    // �v�Z: �J�����ʒu + (�O�� * ����) + (�E���� * ���Y��) + (����� * �c�Y��)
    XMVECTOR vPos = vEye + (vFront * dist) + (vRight * offsetX) + (vUp * offsetY);

    XMFLOAT3 finalPos;
    XMStoreFloat3(&finalPos, vPos);

    hpBar->SetPosition(finalPos);

    if (hpNumber)
    {
        // �J������Right�x�N�g�����g���̂ŁA�ǂ��������Ă��Ă��������u���v�ɂȂ�܂��B
        float numShiftX = -0.55f; // �o�[��肳��ɍ��� (�l��傫������Ƃ����ƍ���)
        float numShiftY = 0.13f; // �o�[��菭�����

        XMVECTOR vNumPos = vPos + (vRight * numShiftX) + (vUp * numShiftY);

        XMFLOAT3 finalNumPos;
        XMStoreFloat3(&finalNumPos, vNumPos);

        hpNumber->SetPosition(finalNumPos);

    }

    // --------------------------------------------------------
    // B. HP�X�V����
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
        //    // Player�̃R���{����UI�ɗ������ށi�ω��������UI���ŃA�j������j
        //    comboUI->SetCombo(player->GetComboCount());
        //}

    }
}