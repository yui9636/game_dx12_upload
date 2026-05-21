#pragma once
#include "Terrain/TerrainBrush.h"
#include "Entity/Entity.h"

class Registry;

// エディタの Terrain 専用パネル。
// EditorLayer が m_showTerrainEditor フラグで表示を制御する。
class TerrainEditorPanel {
public:
    void Draw(Registry& registry, EntityID selectedEntity);
    bool WantsSceneBrush() const { return m_sceneBrushEnabled; }
    float GetBrushRadius() const { return m_brush.radius; }
    void ApplyBrush(Registry& registry, EntityID entity,
                    float worldHitX, float worldHitZ);

    // ---- Automation API ----
    const TerrainBrush& GetBrush()    const { return m_brush; }
    TerrainBrush&       GetBrushMutable()   { return m_brush; }
    bool  IsSceneBrushEnabled()       const { return m_sceneBrushEnabled; }
    void  SetSceneBrushEnabled(bool v)      { m_sceneBrushEnabled = v; }

private:
    TerrainBrush m_brush;
    bool         m_sceneBrushEnabled = false;
};
