#pragma once
#include "Component/Component.h"
#include <vector>
#include <string>
#include <memory>
#include "Storage/GameplayAsset.h"
#include "JSONManager.h"

class Actor;
class Model;

class PlayerEditorComponent : public Component
{
public:
    const char* GetName() const override;

    void Start() override;
    void Update(float dt) override;
    void Render() override;
    void OnGUI() override;

    int GetSelectedNodeIndex() const { return selectedNodeIndex; }
private:
    void DrawMainMenu();
    void DrawAnimationsWindow();
    void DrawHierarchyWindow();
    void DrawEventsWindow();
    void DrawNodeRecursive(::Model* model, int nodeIndex);

    void LoadModelFromDialog();
    void AddRecentModel(const std::string& path);
    void LoadRecents();
    void SaveRecents();

    void ApplyScrubToModel();
    float ComputeApproxActorRadiusByNodes(const ::Model* model) const;
    void AutoScaleActorForPreview();

    // タイムラインデータ管理
    void SaveTimelineToCache(int animIndex);
    void LoadTimelineFromCache(int animIndex);
    void ResizeCache(int size);

    // ★追加: ダイアログ付きの保存・読み込み
    void SaveGameplayDataAs();
    void LoadGameplayDataOpen();

private:
    std::vector<std::string> recentModelPaths;
    JSONManager              recentStore{ "Data/Editor/PlayerEditorRecents.json" };

    char searchText[64] = "";
    int  selectedAnimation = -1;
    int  selectedNodeIndex = -1;
    float clipLength = 1.0f;

    bool autoScaleOnModelLoad = false;
    float previewDesiredRadius = 2.0f;

    std::string currentModelName = "";
    std::vector<std::vector<GESequencerItem>> cachedTimelines;

    std::vector<GECurveSettings> cachedCurves;
};