#pragma once

#include <string>
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include <algorithm>
#include "Effect/EffectCurve.h"



struct RenderContext;


struct EffectTransform
{
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f }; // Euler�p (�x���@)
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
};

// �m�[�h�̎�ނ����ʂ���^�O
enum class EffectNodeType
{
    Empty,      // ��m�[�h�i�e�Ƃ��Ďg���A��]���Ȃǁj
    MeshEmitter, // ���b�V����`�悷��m�[�h
    Particle    //�p�[�e�B�N��
};

class EffectNode
{
public:
    // �R���X�g���N�^ / �f�X�g���N�^
    EffectNode() = default;
    virtual ~EffectNode() = default;

    // --------------------------------------------------------
    // 1. �K�w�\�� (Hierarchy)
    // --------------------------------------------------------
    std::string name = "Node";          // �m�[�h�� (Editor�\���p)
    EffectNode* parent = nullptr;       // �e�ւ̃|�C���^ (�v�Z�p)

    EffectNodeType type = EffectNodeType::Empty;

    // �q�m�[�h���� (���L��������)
    std::vector<std::shared_ptr<EffectNode>> children;

    // �q��ǉ�����֐�
    void AddChild(std::shared_ptr<EffectNode> child)
    {
        child->parent = this;
        children.push_back(child);
    }

    // --------------------------------------------------------
    // 2. �g�����X�t�H�[�� (Transform)
    // --------------------------------------------------------

    // �y���[�J�����W�z: �^�C�����C����Inspector�ł�����l
    EffectTransform localTransform;

    EffectCurve positionCurves[3];
    EffectCurve rotationCurves[3];
    EffectCurve scaleCurves[3];

    // �y���[���h�s��z: �`��Ɏg���ŏI�v�Z����
    DirectX::XMFLOAT4X4 worldMatrix = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };

    // --------------------------------------------------------
    // 3. �X�V���� (Update)
    // --------------------------------------------------------

    /// @brief �s����ċA�I�ɍX�V����
    /// @param parentMatrix �e�̃��[���h�s�� (���[�g�̏ꍇ�͒P�ʍs��)
    void UpdateTransform(const DirectX::XMMATRIX& parentMatrix)
    {
        using namespace DirectX;

        // 1. ���[�J���s������ (S * R * T)
        XMMATRIX S = XMMatrixScaling(localTransform.scale.x, localTransform.scale.y, localTransform.scale.z);

        // ��] (Euler -> Matrix)
        XMMATRIX R = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(localTransform.rotation.x),
            XMConvertToRadians(localTransform.rotation.y),
            XMConvertToRadians(localTransform.rotation.z)
        );

        XMMATRIX T = XMMatrixTranslation(localTransform.position.x, localTransform.position.y, localTransform.position.z);

        XMMATRIX localM = S * R * T;

        // 2. �e�̍s����|���āu���[���h�s��v�ɂ��� (����ԕR�̌v�Z)
        XMMATRIX worldM = localM * parentMatrix;

        // 3. ���ʂ�ۑ�
        XMStoreFloat4x4(&worldMatrix, worldM);

        // 4. �q�ǂ������ɂ��u���̎��̏ꏊ�v��n���Čv�Z������
        for (auto& child : children)
        {
            child->UpdateTransform(worldM);
        }
    }

    // --------------------------------------------------------
    // 4. ���z�֐� (Polymorphism)
    // --------------------------------------------------------

    // ��ނ��擾 (�h���N���X�ŏ㏑������)
    virtual EffectNodeType GetType() const { return EffectNodeType::Empty; }

 /*   virtual void Update(float deltaTime) {
   
    }


    virtual void Render(RenderContext& rc) {}
*/
    virtual void Update(float deltaTime) {
        for (auto& child : children) {
            child->Update(deltaTime);
        }
    }

    // ���C��2: �q�������� Render ���Ă�
    virtual void Render(RenderContext& rc) {
        for (auto& child : children) {
            child->Render(rc);
        }
    }

    virtual void Reset() {
        // �������g�̃��Z�b�g�������K�v�Ȃ炱���ɏ����i�J�[�u�̃L���b�V���N���A�Ȃǁj

        // �q�m�[�h�֓`�d
        for (auto& child : children) {
            child->Reset();
        }
    }

    virtual void UpdateWithAge(float age, float lifeTime) {
        // 1. �m�[�h�̐��K������ (0.0 ~ 1.0) ���v�Z
        float nodeLocalTime = age - startTime;
        float t = 0.0f;
        if (duration > 0.001f) {
            t = nodeLocalTime / duration;
        }

        // �͈͊O�̓N�����v
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        // 2. �J�[�u���L���i�L�[�t���[�������݂���j�ꍇ�̂݁AlocalTransform���㏑��
        // ����ɂ��A�J�[�u���g��Ȃ��m�[�h�̓C���X�y�N�^�[�̌Œ�l�����̂܂܎g���܂�

        // Position
        if (positionCurves[0].IsValid()) localTransform.position.x = positionCurves[0].Evaluate(t);
        if (positionCurves[1].IsValid()) localTransform.position.y = positionCurves[1].Evaluate(t);
        if (positionCurves[2].IsValid()) localTransform.position.z = positionCurves[2].Evaluate(t);

        // Rotation
        if (rotationCurves[0].IsValid()) localTransform.rotation.x = rotationCurves[0].Evaluate(t);
        if (rotationCurves[1].IsValid()) localTransform.rotation.y = rotationCurves[1].Evaluate(t);
        if (rotationCurves[2].IsValid()) localTransform.rotation.z = rotationCurves[2].Evaluate(t);

        // Scale
        if (scaleCurves[0].IsValid()) localTransform.scale.x = scaleCurves[0].Evaluate(t);
        if (scaleCurves[1].IsValid()) localTransform.scale.y = scaleCurves[1].Evaluate(t);
        if (scaleCurves[2].IsValid()) localTransform.scale.z = scaleCurves[2].Evaluate(t);

        // �q�m�[�h�����l�ɍX�V
        for (auto& child : children) {
            child->UpdateWithAge(age, lifeTime);
        }
    }

public:
    float startTime = 0.0f;   // �o���J�n���� (�b)
    float duration = 5.0f;    // �\������ (�b)
    bool isVisible = true;    // �G�f�B�^��ł̕\��/��\���t���O

};