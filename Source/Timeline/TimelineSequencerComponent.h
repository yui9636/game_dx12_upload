#pragma once
#include "Component/Component.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <imgui.h>
#include <DirectXMath.h> 
#include "Storage/GameplayAsset.h"

class CameraController;
class RunnerComponent;
class ParticleManager;
class EffectManager;

class TimelineSequencerComponent final : public Component
{
public:
    const char* GetName() const override { return "TimelineSequencer"; }

public:
    void Start() override;
    void CalcWorldTRForItem(const GESequencerItem& it, DirectX::XMFLOAT3& outPos, DirectX::XMFLOAT4& outRot, DirectX::XMFLOAT3& outScale);
    void Update(float dt) override;
    void OnGUI() override;

    void OnGizmo();

    void SetRunner(RunnerComponent* r) { runner = r; }
    void SetFps(float inFps) { fps = (inFps > 1.0f ? inFps : 1.0f); }
    float GetFps() const { return fps; }

    void SetAnimationIndex(int idx) { animationIndex = idx; }
    int  GetAnimationIndex() const { return animationIndex; }

    float GetClipLengthSeconds() const;
    int   GetCurrentFrame() const { return currentFrame; }

    void BindAnimation(int animIndex, float clipLengthSec);

    const std::vector<GESequencerItem>& GetItems() const;
    std::vector<GESequencerItem>& GetItemsMutable();

    // ★追加: カーブ設定の取得・反映
    GECurveSettings GetCurveSettings() const;
    void SetCurveSettings(const GECurveSettings& settings);

    // ★修正: privateからpublicへ移動 (外部から呼べるようにする)
    int   SecondsToFrames(float seconds) const;
    float FramesToSeconds(int frames) const;

    void SetCameraController(CameraController* camCtrl) { targetCamera = camCtrl; }

    RunnerComponent* GetRunner() const { return runner; } // Runnerへのアクセス用
    const GESequencerItem* GetActiveShakeItem() const;     // 現在有効なシェイク設定の検索用
private:
    void  SanitizeItems();
    void  LoadSpeedCurvePointsFromRunner();
    void  StoreSpeedCurvePointsToRunner();

    DirectX::XMMATRIX CalcWorldMatrixForItem(const GESequencerItem& item);
    void DrawGizmoForItem(GESequencerItem& item);

private:
    RunnerComponent* runner = nullptr;
    CameraController* targetCamera = nullptr;

    int   frameMin = 0;
    int   frameMax = 600;
    int   currentFrame = 0;
    int   selectedEntry = -1;
    int   firstVisible = 0;
    bool  sequencerOpen = true;

    float fps = 60.0f;
    int   animationIndex = -1;
    float localClipLengthSec = 10.0f;

    std::vector<std::string> typeNames;
    std::vector<GESequencerItem> items;

    bool  uiSnapEnabled = true;
    int   uiSnapStep = 1;
    bool  previewDrive = false;

    // カーブ編集用メンバ
    bool  uiSpeedCurveEnabled = false;
    bool  uiSpeedCurveUseRangeSpace = false;
    std::vector<ImVec2> uiSpeedCurvePoints;
};