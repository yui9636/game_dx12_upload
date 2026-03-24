#pragma once

#include <DirectXMath.h>
#include <memory>
#include "Character.h"              
#include "Model/Model.h"
#include "Model/ModelRenderer.h"
#include "Input/Input.h"
#include "Camera/Camera.h"
#include <imgui.h>
#include "Input/InputActionComponent.h"  

#include "Storage/GameplayAsset.h"
#include "C_DodgeGauge.h"
#include <set>

class InputActionComponent;
class LocomotionComponent;
class AnimatorComponent;
class RunnerComponent;
class TimelineSequencerComponent;
class CameraEditorComponent;
class GEStorageCompilerComponent;

class Player : public Character
{
public:
    Player();
    ~Player() override = default;

    void Initialize(ID3D11Device* device) override;

    void Start() override;
    void Update(float dt) override;

    void Render(ModelRenderer* renderer) override;

    void OnGUI() override;

    void OnTriggerEnter(Actor* other, const Collider* selfCol, const Collider* otherCol) override;

    void AddCombo();
    int GetComboCount() const { return comboCount; }

    virtual std::string GetTypeName() const override { return "Player"; }

    bool IsCharacter() const override { return true; }
public:
    enum Animation
    {
        Intro, 
        Idle,
        Walk_Back, 
        Walk_Back_L45, 
        Walk_Back_L90, 
        Walk_Back_R45,
        Walk_Back_R90,
        Walk_Front,
        Walk_Front_L45,
        Walk_Front_L90,
        Walk_Front_R45, 
        Walk_Front_R90,
        Walk_L90, 
        Walk_R90,
        Jogging_B, 
        Jogging_BL45, 
        Jogging_BR45, 
        Jogging_F, 
        Jogging_FL45, 
        Jogging_FR45, 
        Jogging_L90, 
        Jogging_R90,
        Run, 
        Run_Fast,
        Run_injured,
        Run_Fast_ldle,
        Walk_Idle,
        Idle_To_Jog_Trun_L90, 
        Idle_To_Jog_Trun_R90, 
        Idle_To_Run_Trun_L90, 
        Idle_To_Run_Trun_R90,
        Idle_To_Walk_Trun_L90,
        Idle_Trun_L90, 
        Idle_Trun_R90,
        Jog_Idle_Turn_L90, 
        Jog_Idle_Turn_R90, 
        Jog_Jog_Turn_L90, 
        Jog_Jog_Turn_R90,
        Turn_in_place_Idle_to_jog, 
        Turn_in_place_Idle_to_Run,
        Turn_in_place_Idle_to_Walk,
        Turn_in_place_jog_to_Run, 
        Turn_in_place_jog_to_jog, 
        Turn_in_place_Run_to_Run,
        Turn_in_place_Walk_to_jog, 
        Turn_in_place_Walk_to_Run, 
        Turn_in_place_Walk_to_Walk,
        Walk_Idle_Turn_L90,
        Walk_Idle_Turn_R90,
        Walk_to_Walk_Tutn_L90, 
        Walk_to_Walk_Tutn_R90,
        Damage_Front_Big,
        Damage_Front_Down_StandUp, 
        Damage_Front_Down_Loop, 
        Damage_Front_High, 
        Damage_Front_Small,
        Damage_Back_Down_Loop, 
        Damage_Back_Down_Smash,
        Damage_Back_Down_StandUp,
        Damage_Back_Small,
        Damage_Left_Small,
        Damage_Right_Small,
        KnockDown_Back_BW_Rolling_StandUp,
        KnockDown_Back_FW_Rolling_StandUp,
        KnockDown_Front_BW_Rolling_StandUp,
        KnockDown_Front_FW_Rolling_StandUp,
        Die, 
        Guard, 
        Guard_Attack, 
        Guard_End, 
        Guard_Start, 
        StandUp, 
        Idle_To_Walk_Trun_R90,
        Dodge_Back, 
        Dodge_Front,
        Combo1,
        Combo2,
        Combo3, 
        Combo4, 
        Combo5, 
        Combo6, 
        Combo7, 
        Combo8, 
        Combo9, 
        Combo10, 
        Combo11, 
        Combo12,
        Combo13, 
        Combo14, 
        Combo15, 
        Combo16, 
        Combo17, 
        Combo18, 
        Combo19, 
        Combo20, 
        Combo21, 
        Combo22, 
        Combo23, 
        Combo24,
        DashAttack1_root,
        DashAttack2_root,
        JumpAttack1_root,
        JumpAttack2_root,
        JumpAttack3_root,
        UpperAttack_root,
        Skill_Attack1_root,
        Skill_Attack2_root,
        Skill_Attack3_root,
        Skill_Attack4_root,
        Skill_Attack5_root,
        Skill_Attack6_root,
        Skill_Attack7_root,
        Skill_Attack8_root,
        Skill_Attack9_root,
        Skill_Attack10_root,
        Skill_Attack11_root,
        Skill_Attack12_root,
        Skill_Attack13,
        Skill_Attack14_root,

    };

    struct ActionNode
    {
        int   animIndex;

        int   nextLight = -1;
        int   nextHeavy = -1;
        int   nextDodge = -1;

        float inputStart = 0.0f;
        float inputEnd = 1.0f;
        float comboStart = 0.5f;
        float cancelStart = 0.2f;

        float magnetismRange = 10.0f;
        float magnetismSpeed = 0.0f;

        int   damageVal = 0;

        float animSpeed = 1.0f;
    };

private:
    enum class State {
        Locomotion, Action, Dodge, Jump, Damage, Dead
    };

    void PlayAction(int actionNodeIndex);
    void UpdateLocomotion(float dt);
    void SyncLocomotionAnimation();
    void UpdateAction(float dt);
    bool TryDodge();

    void BuildActionDatabase();

    void RotateToNearestEnemyInstant();
    void UpdateMagnetism(float dt);

    bool IsAttackAction(int animIndex) const;

protected:
    void OnDamaged() override;
    void OnDead() override;

private:
    State state = State::Locomotion;

    std::vector<ActionNode> actionDatabase;
    int currentActionIdx = -1;
    int reservedActionIdx = -1;
    ActionNode currentActionData;

    float verticalVelocity = 0.0f;
    float customGravity = -30.0f;

    float stageLimitRadius = 50.0f;

    bool isMagnetismActive = false;
    DirectX::XMFLOAT3 magTargetPos = { 0,0,0 };
    float magnetismTimer = 0.0f;
    float magnetismDuration = 0.0f;

    bool isJustDodgeAccept = false;
    float dodgeMoveScale = 5.0f;
    int comboCount = 0;
    float comboTimer = 0.0f;
    const float COMBO_TIMEOUT = 3.0f;

    std::set<Actor*> hitList;
    int lastHitboxStart = -1;

    float stateTimer = 0.0f;
    GameplayAsset gameplayData;
    std::shared_ptr<InputActionComponent> input;
    std::shared_ptr<LocomotionComponent>  locomotion;
    std::shared_ptr<AnimatorComponent>    animator;
    std::shared_ptr<RunnerComponent>      runner;
    std::shared_ptr<TimelineSequencerComponent> sequencer;
    std::shared_ptr<CameraEditorComponent> cameraEditor;
    std::shared_ptr<C_DodgeGauge> dodgeGauge;
};
