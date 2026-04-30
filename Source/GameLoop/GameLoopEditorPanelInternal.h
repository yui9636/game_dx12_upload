#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <filesystem>
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "GameLoopAsset.h"
#include "GameLoopScenePicker.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <system_error>

#include <imgui.h>
#include <imgui_internal.h>


namespace
{
    constexpr float NodeW = 260.0f;
    constexpr float NodeH = 92.0f;
    constexpr float PinR = 7.0f;
    constexpr const char* PickerPopup = "GameLoop Scene Picker";
    constexpr const char* ConditionPopup = "GameLoop Condition Preset";

    float MinF(float a, float b) { return a < b ? a : b; }
    float MaxF(float a, float b) { return a > b ? a : b; }
    float ClampF(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

    ImVec2 Lerp(const ImVec2& a, const ImVec2& b, float t)
    {
        return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
    }

    ImVec2 Bezier(const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, float t)
    {
        ImVec2 a = Lerp(p0, p1, t);
        ImVec2 b = Lerp(p1, p2, t);
        ImVec2 c = Lerp(p2, p3, t);
        ImVec2 d = Lerp(a, b, t);
        ImVec2 e = Lerp(b, c, t);
        return Lerp(d, e, t);
    }

    float DistSqSeg(const ImVec2& p, const ImVec2& a, const ImVec2& b)
    {
        float vx = b.x - a.x;
        float vy = b.y - a.y;
        float wx = p.x - a.x;
        float wy = p.y - a.y;
        float len = vx * vx + vy * vy;
        float t = len > 0.0001f ? (wx * vx + wy * vy) / len : 0.0f;
        t = ClampF(t, 0.0f, 1.0f);
        float dx = p.x - (a.x + vx * t);
        float dy = p.y - (a.y + vy * t);
        return dx * dx + dy * dy;
    }

    bool NearBezier(const ImVec2& p, const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, float r)
    {
        ImVec2 prev = p0;
        float rr = r * r;
        for (int i = 1; i <= 28; ++i) {
            ImVec2 cur = Bezier(p0, p1, p2, p3, (float)i / 28.0f);
            if (DistSqSeg(p, prev, cur) <= rr) return true;
            prev = cur;
        }
        return false;
    }

    bool PointInCircle(const ImVec2& p, const ImVec2& c, float r)
    {
        float x = p.x - c.x;
        float y = p.y - c.y;
        return x * x + y * y <= r * r;
    }

    const char* ActionName(int i)
    {
        if (i == 0) return "Confirm";
        if (i == 1) return "Cancel";
        if (i == 2) return "Retry";
        return "Input";
    }

    const char* ActorName(ActorType t)
    {
        if (t == ActorType::Player) return "Player";
        if (t == ActorType::Enemy) return "Enemy";
        if (t == ActorType::NPC) return "NPC";
        if (t == ActorType::Neutral) return "Neutral";
        return "None";
    }

    ImU32 StatusColor(int s)
    {
        if (s >= 2) return IM_COL32(230, 76, 86, 255);
        if (s == 1) return IM_COL32(230, 180, 72, 255);
        return IM_COL32(82, 190, 118, 255);
    }

    const char* StatusText(int s)
    {
        if (s >= 2) return "E";
        if (s == 1) return "W";
        return "OK";
    }

    void DrawTextScaled(ImDrawList* dl, const ImVec2& pos, ImU32 color, const char* text, float zoom)
    {
        float fontSize = ImGui::GetFontSize() * ClampF(zoom, 0.70f, 1.20f);
        dl->AddText(ImGui::GetFont(), fontSize, pos, color, text);
    }

    void DrawArrow(ImDrawList* dl, const ImVec2& tip, const ImVec2& prev, ImU32 col, float zoom)
    {
        float vx = tip.x - prev.x;
        float vy = tip.y - prev.y;
        float len = std::sqrt(vx * vx + vy * vy);
        if (len < 0.001f) return;
        vx /= len;
        vy /= len;
        float nx = -vy;
        float ny = vx;
        float back = 15.0f * zoom;
        float half = 6.5f * zoom;
        ImVec2 a(tip.x - vx * back + nx * half, tip.y - vy * back + ny * half);
        ImVec2 b(tip.x - vx * back - nx * half, tip.y - vy * back - ny * half);
        dl->AddTriangleFilled(tip, a, b, col);
    }
}

class GameLoopEditorPanelInternal
{
public:
    GameLoopEditorPanelInternal();
    void Draw(bool* p_open = nullptr, bool* outFocused = nullptr);
    const std::filesystem::path& GetCurrentPath() const { return m_currentPath; }
    void SetCurrentPath(const std::filesystem::path& p) { m_currentPath = p; }

private:
    enum class SelectionKind { None, Node, Transition, Condition };
    enum class PickerMode { None, CreateNode, ReplaceNode, CreateNodeAndTransition };

    struct NodeView
    {
        uint32_t id = 0;
        DirectX::XMFLOAT2 pos = { 0.0f, 0.0f };
    };

    void DrawToolbar();
    void DrawMainLayout();
    void DrawGraph(const ImVec2& size);
    void DrawInspector();
    void DrawValidateSummary();
    void DrawNode(GameLoopNode& node, const ImVec2& origin);
    void DrawTransition(int index, const ImVec2& origin);
    void DrawScenePickerPopup();
    void DrawConditionPresetPopup();
    void DrawGraphContextMenu();
    void DrawNodeContextMenu(GameLoopNode& node);
    void DrawTransitionContextMenu(int index);

    void OpenPickerForCreate(const DirectX::XMFLOAT2& pos);
    void OpenPickerForReplace(uint32_t nodeId);
    void OpenPickerForConnection(uint32_t fromNodeId, const DirectX::XMFLOAT2& pos);
    void AddSceneNode(const std::string& scenePath, const DirectX::XMFLOAT2& pos);
    void ReplaceNodeScene(uint32_t nodeId, const std::string& scenePath);
    void AddTransition(uint32_t fromNodeId, uint32_t toNodeId);
    void AddConditionPreset(GameLoopTransition& transition, int presetIndex);

    GameLoopNode* FindNode(uint32_t id);
    const GameLoopNode* FindNode(uint32_t id) const;
    NodeView* FindView(uint32_t id);
    const NodeView* FindView(uint32_t id) const;
    DirectX::XMFLOAT2& GetOrCreateNodePos(uint32_t id);
    GameLoopTransition* SelectedTransition();
    int SelectedTransitionIndex() const;

    void SelectNode(uint32_t id);
    void SelectTransition(int index);
    void ClearSelection();
    void DeleteSelected();
    void DeleteNode(uint32_t id);
    void DeleteTransition(int index);
    void ReverseTransition(int index);
    void FitGraph(const ImVec2& size);
    void Validate();
    void Save();
    void Load(const std::filesystem::path& path);

    ImVec2 GraphToScreen(const DirectX::XMFLOAT2& p, const ImVec2& origin) const;
    DirectX::XMFLOAT2 ScreenToGraph(const ImVec2& p, const ImVec2& origin) const;
    std::string ConditionLabel(const GameLoopCondition& c) const;
    std::string TransitionLabel(const GameLoopTransition& t) const;
    std::string Ellipsis(const std::string& text, float width) const;
    int SceneStatus(const GameLoopNode& node) const;

private:
    GameLoopAsset m_asset;
    std::vector<NodeView> m_nodeViews;
    std::filesystem::path m_currentPath = "Data/GameLoop/Main.gameloop";

    SelectionKind m_selection = SelectionKind::None;
    uint32_t m_selectedNodeId = 0;
    int m_selectedTransitionIndex = -1;
    int m_selectedConditionIndex = -1;

    uint32_t m_hoveredNodeId = 0;
    int m_hoveredTransitionIndex = -1;
    bool m_connecting = false;
    bool m_connectionDragged = false;
    uint32_t m_connectFromNodeId = 0;

    DirectX::XMFLOAT2 m_graphOffset = { 180.0f, 120.0f };
    float m_graphZoom = 1.0f;
    bool m_fitRequested = true;
    DirectX::XMFLOAT2 m_contextGraphPos = { 0.0f, 0.0f };

    GameLoopScenePicker m_scenePicker;
    PickerMode m_pickerMode = PickerMode::None;
    uint32_t m_pickerTargetNodeId = 0;
    uint32_t m_pickerFromNodeId = 0;
    DirectX::XMFLOAT2 m_pickerGraphPos = { 0.0f, 0.0f };

    GameLoopValidateResult m_validateResult;
    bool m_validated = false;
    bool m_dirty = false;
    char m_loadPath[512] = {};
    char m_saveAsPath[512] = {};
};
