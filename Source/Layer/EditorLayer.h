#pragma once
#include "Layer.h"
#include "GameLayer.h"
#include "Asset/AssetBrowser.h"
#include <memory>

class EditorLayer : public Layer
{
public:
    EditorLayer(GameLayer* gameLayer);
    ~EditorLayer() override = default;

    void Initialize() override;
    void Finalize() override;
    void Update(const EngineTime& time) override;
    void RenderUI() override;

    AssetBrowser* GetAssetBrowser() const { return m_assetBrowser.get(); }
private:
    GameLayer* m_gameLayer;
    std::unique_ptr<AssetBrowser> m_assetBrowser;

    EntityID m_selectedEntity = Entity::NULL_ID;
    bool m_showLightingWindow = false;
    bool m_showGBufferDebug = false;
    // ==========================================
    // ==========================================
    void DrawDockSpace();
    void DrawMenuBar();
    void DrawMainToolbar();
    void DrawSceneView();
    void DrawHierarchy();
    void DrawInspector();
    void DrawLightingWindow();
    void DrawGBufferDebugWindow();
};
