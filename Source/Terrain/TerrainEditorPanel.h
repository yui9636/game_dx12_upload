#pragma once
#include "Terrain/TerrainBrush.h"
#include "Entity/Entity.h"

class Registry;

// エディタの Terrain 専用パネル。
// EditorLayer が m_showTerrainEditor フラグで表示を制御する。
class TerrainEditorPanel {
public:
    void Draw(Registry& registry, EntityID selectedEntity);

private:
    void DrawGenerateTab(Registry& registry, EntityID entity);
    void DrawSculptTab(Registry& registry, EntityID entity);
    void DrawPaintTab(Registry& registry, EntityID entity);
    void DrawLayersTab(Registry& registry, EntityID entity);
    void DrawSettingsTab(Registry& registry, EntityID entity);

    void ApplyBrush(Registry& registry, EntityID entity,
                    float worldHitX, float worldHitZ);

    TerrainBrush m_brush;
    int          m_activeTab = 0;
};
