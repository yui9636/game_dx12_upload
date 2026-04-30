#include "GameLoopEditorPanelInternal.h"
#include <cfloat>

namespace
{
    
    
    uint32_t g_draggingGameLoopNodeId = 0;

    
    float CalcSafeGraphTextFontSize(float zoom)
    {
        return ImGui::GetFontSize() * ClampF(zoom, 0.10f, 4.00f);
    }

    
    float GetSafeBaseFontSize()
    {
        const float fontSize = ImGui::GetFontSize();
        return fontSize > 0.0f ? fontSize : 13.0f;
    }

    
    ImVec2 CalcSafeTextSizeAtFontSize(const char* text, float fontSize)
    {
        if (!text || fontSize <= 0.0f || !ImGui::GetFont()) {
            return ImVec2(0.0f, 0.0f);
        }

        const float baseFontSize = GetSafeBaseFontSize();
        const float visualScale = fontSize / baseFontSize;
        const ImVec2 baseSize = ImGui::GetFont()->CalcTextSizeA(baseFontSize, FLT_MAX, 0.0f, text);
        return ImVec2(baseSize.x * visualScale, baseSize.y * visualScale);
    }

    
    void ScaleSafeTextVertices(ImDrawList* drawList, int vertexStart, const ImVec2& origin, float visualScale)
    {
        if (!drawList || visualScale <= 0.0f) {
            return;
        }
        if (std::fabs(visualScale - 1.0f) <= 0.0001f) {
            return;
        }

        for (int i = vertexStart; i < drawList->VtxBuffer.Size; ++i) {
            ImDrawVert& vertex = drawList->VtxBuffer[i];
            vertex.pos.x = origin.x + (vertex.pos.x - origin.x) * visualScale;
            vertex.pos.y = origin.y + (vertex.pos.y - origin.y) * visualScale;
        }
    }

    
    
    void DrawSafeTextAtFontSize(ImDrawList* drawList, const ImVec2& position, ImU32 color, const char* text, float fontSize)
    {
        if (!drawList || !text || fontSize <= 0.0f || !ImGui::GetFont()) {
            return;
        }

        const float baseFontSize = GetSafeBaseFontSize();
        const float visualScale = fontSize / baseFontSize;
        const int vertexStart = drawList->VtxBuffer.Size;
        drawList->AddText(ImGui::GetFont(), baseFontSize, position, color, text);
        ScaleSafeTextVertices(drawList, vertexStart, position, visualScale);
    }

    
    std::string BuildSafeMiddleEllipsisAtFontSize(const std::string& text, float maxWidth, float fontSize)
    {
        if (text.empty() || maxWidth <= 0.0f || fontSize <= 0.0f) {
            return std::string();
        }
        if (CalcSafeTextSizeAtFontSize(text.c_str(), fontSize).x <= maxWidth) {
            return text;
        }

        const std::string ellipsis = "...";
        if (CalcSafeTextSizeAtFontSize(ellipsis.c_str(), fontSize).x > maxWidth) {
            return std::string();
        }

        const int textLength = static_cast<int>(text.size());
        for (int keepCount = textLength - 1; keepCount > 0; --keepCount) {
            int leftCount = keepCount / 2;
            int rightCount = keepCount - leftCount;
            if (leftCount < 1) {
                leftCount = 1;
            }
            if (rightCount < 1) {
                rightCount = 1;
            }
            if (leftCount + rightCount >= textLength) {
                continue;
            }

            const std::string candidate = text.substr(0, static_cast<size_t>(leftCount))
                + ellipsis
                + text.substr(static_cast<size_t>(textLength - rightCount));
            if (CalcSafeTextSizeAtFontSize(candidate.c_str(), fontSize).x <= maxWidth) {
                return candidate;
            }
        }

        return ellipsis;
    }

    
    void DrawSafeTextScaled(ImDrawList* drawList, const ImVec2& position, ImU32 color, const char* text, float zoom)
    {
        DrawSafeTextAtFontSize(drawList, position, color, text, CalcSafeGraphTextFontSize(zoom));
    }

    
    float CalcLargeSceneNameFontSize(const std::string& sceneName, float maxWidth, float maxHeight, float zoom)
    {
        if (sceneName.empty() || maxWidth <= 1.0f || maxHeight <= 1.0f || zoom <= 0.0f) {
            return CalcSafeGraphTextFontSize(zoom);
        }

        const float zoomScale = ClampF(zoom, 0.10f, 4.00f);
        float low = ImGui::GetFontSize() * 0.70f * zoomScale;
        float high = ImGui::GetFontSize() * 2.65f * zoomScale;
        high = MinF(high, maxHeight * 0.90f);
        if (high < low) {
            low = high;
        }

        float result = low;
        for (int i = 0; i < 18; ++i) {
            const float mid = (low + high) * 0.5f;
            const std::string visibleText = BuildSafeMiddleEllipsisAtFontSize(sceneName, maxWidth, mid);
            const ImVec2 textSize = CalcSafeTextSizeAtFontSize(visibleText.c_str(), mid);
            const bool fits = !visibleText.empty() && textSize.x <= maxWidth && textSize.y <= maxHeight;
            if (fits) {
                result = mid;
                low = mid;
            } else {
                high = mid;
            }
        }

        return result;
    }

    
    std::string BuildSceneDisplayName(const GameLoopNode& node)
    {
        if (!node.scenePath.empty()) {
            std::filesystem::path scenePath(node.scenePath);
            const std::string stem = scenePath.stem().string();
            if (!stem.empty()) {
                return stem;
            }
        }
        if (!node.name.empty()) {
            return node.name;
        }
        return "Scene";
    }
}


void GameLoopEditorPanelInternal::DrawGraph(const ImVec2& sizeIn)
{
    ImVec2 size(MaxF(sizeIn.x, 200.0f), MaxF(sizeIn.y, 200.0f));
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 end(origin.x + size.x, origin.y + size.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (m_fitRequested) {
        FitGraph(size);
        m_fitRequested = false;
    }

    m_hoveredNodeId = 0;
    m_hoveredTransitionIndex = -1;

    dl->PushClipRect(origin, end, true);
    dl->AddRectFilled(origin, end, IM_COL32(20, 22, 27, 255));

    float grid = 64.0f * m_graphZoom;
    if (grid >= 18.0f) {
        float ox = std::fmod(m_graphOffset.x, grid);
        float oy = std::fmod(m_graphOffset.y, grid);
        for (float x = origin.x + ox; x < end.x; x += grid) {
            dl->AddLine(ImVec2(x, origin.y), ImVec2(x, end.y), IM_COL32(55, 58, 68, 120));
        }
        for (float y = origin.y + oy; y < end.y; y += grid) {
            dl->AddLine(ImVec2(origin.x, y), ImVec2(end.x, y), IM_COL32(55, 58, 68, 120));
        }
    }

    
    
    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("GraphCanvas", size, ImGuiButtonFlags_MouseButtonMiddle);
    const bool canvasHovered = ImGui::IsMouseHoveringRect(origin, end, true);

    std::string dropped;
    if (m_scenePicker.AcceptSceneAssetDragDrop(dropped)) {
        AddSceneNode(dropped, ScreenToGraph(ImGui::GetIO().MousePos, origin));
    }

    for (int i = 0; i < static_cast<int>(m_asset.transitions.size()); ++i) {
        DrawTransition(i, origin);
    }

    for (auto& node : m_asset.nodes) {
        DrawNode(node, origin);
    }

    if (m_connecting) {
        NodeView* v = FindView(m_connectFromNodeId);
        if (v) {
            float w = NodeW * m_graphZoom;
            float h = NodeH * m_graphZoom;
            ImVec2 p = GraphToScreen(v->pos, origin);
            ImVec2 a(p.x + w, p.y + h * 0.5f);
            ImVec2 b = ImGui::GetIO().MousePos;
            ImVec2 c1(a.x + 90.0f * m_graphZoom, a.y);
            ImVec2 c2(b.x - 90.0f * m_graphZoom, b.y);
            dl->AddBezierCubic(a, c1, c2, b, IM_COL32(150, 190, 255, 230), 2.5f);
            DrawArrow(dl, b, Bezier(a, c1, c2, b, 0.92f), IM_COL32(150, 190, 255, 230), m_graphZoom);
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
            m_connectionDragged = true;
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if (m_connectionDragged) {
                if (m_hoveredNodeId != 0 && m_hoveredNodeId != m_connectFromNodeId) {
                    AddTransition(m_connectFromNodeId, m_hoveredNodeId);
                }
            }
            m_connecting = false;
            m_connectionDragged = false;
            m_connectFromNodeId = 0;
        }
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        g_draggingGameLoopNodeId = 0;
    }

    if (canvasHovered && !m_connecting) {
        const bool canOperateBackground =
            m_hoveredNodeId == 0 &&
            m_hoveredTransitionIndex < 0 &&
            g_draggingGameLoopNodeId == 0;

        if (canOperateBackground &&
            (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) ||
                (ImGui::IsKeyDown(ImGuiKey_Space) && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)))) {
            m_graphOffset.x += ImGui::GetIO().MouseDelta.x;
            m_graphOffset.y += ImGui::GetIO().MouseDelta.y;
        }

        const float wheel = ImGui::GetIO().MouseWheel;
        if (std::fabs(wheel) > 0.001f) {
            DirectX::XMFLOAT2 before = ScreenToGraph(ImGui::GetIO().MousePos, origin);
            m_graphZoom = ClampF(m_graphZoom * (wheel > 0.0f ? 1.1f : 0.9f), 0.3f, 2.5f);
            m_graphOffset.x = ImGui::GetIO().MousePos.x - origin.x - before.x * m_graphZoom;
            m_graphOffset.y = ImGui::GetIO().MousePos.y - origin.y - before.y * m_graphZoom;
        }

        if (canOperateBackground && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            m_contextGraphPos = ScreenToGraph(ImGui::GetIO().MousePos, origin);
            ImGui::OpenPopup("GraphContext");
        }

        if (canOperateBackground &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsKeyDown(ImGuiKey_Space)) {
            ClearSelection();
        }
    }

    if (m_asset.nodes.empty()) {
        dl->AddText(ImVec2(origin.x + 20.0f, origin.y + 20.0f), IM_COL32(160, 170, 190, 255), "Empty Scene Flow: right click or drop .scene file.");
    }

    DrawGraphContextMenu();
    dl->PopClipRect();
}


void GameLoopEditorPanelInternal::DrawNode(GameLoopNode& node, const ImVec2& origin)
{
    DirectX::XMFLOAT2& gp = GetOrCreateNodePos(node.id);
    ImVec2 pos = GraphToScreen(gp, origin);

    float w = NodeW * m_graphZoom;
    float h = NodeH * m_graphZoom;
    float pin = PinR * ClampF(m_graphZoom, 0.75f, 1.5f);
    float round = 8.0f * ClampF(m_graphZoom, 0.75f, 1.5f);

    ImVec2 max(pos.x + w, pos.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    bool selected = m_selection == SelectionKind::Node && m_selectedNodeId == node.id;
    int status = SceneStatus(node);
    ImU32 border = selected ? IM_COL32(120, 185, 255, 255) : IM_COL32(84, 90, 108, 255);

    dl->AddRectFilled(pos, max, selected ? IM_COL32(46, 64, 92, 255) : IM_COL32(34, 37, 46, 255), round);
    dl->AddRect(pos, max, border, round, 0, selected ? 3.0f : 1.5f);

    ImVec2 inputPin(pos.x, pos.y + h * 0.5f);
    ImVec2 outputPin(max.x, pos.y + h * 0.5f);

    dl->AddCircleFilled(inputPin, pin, IM_COL32(105, 120, 150, 255));
    dl->AddCircleFilled(outputPin, pin, IM_COL32(125, 175, 255, 255));

    const std::string sceneNameRaw = BuildSceneDisplayName(node);
    const float titleMaxWidth = MaxF(1.0f, w - 34.0f * m_graphZoom);
    const float titleMaxHeight = MaxF(1.0f, h - 46.0f * m_graphZoom);
    const float titleFontSize = CalcLargeSceneNameFontSize(sceneNameRaw, titleMaxWidth, titleMaxHeight, m_graphZoom);
    const std::string sceneName = BuildSafeMiddleEllipsisAtFontSize(sceneNameRaw, titleMaxWidth, titleFontSize);
    const ImVec2 titleSize = CalcSafeTextSizeAtFontSize(sceneName.c_str(), titleFontSize);
    const ImVec2 titlePos(
        pos.x + (w - titleSize.x) * 0.5f,
        pos.y + (h - titleSize.y) * 0.5f);
    DrawSafeTextAtFontSize(dl, titlePos, IM_COL32(246, 249, 255, 255), sceneName.c_str(), titleFontSize);

    const char* statusText = StatusText(status);
    const float badgeFontSize = CalcSafeGraphTextFontSize(m_graphZoom);
    const ImVec2 badgeTextSize = CalcSafeTextSizeAtFontSize(statusText, badgeFontSize);
    const float badgePadX = 7.0f * m_graphZoom;
    const float badgePadY = 3.0f * m_graphZoom;
    const float badgeWidth = badgeTextSize.x + badgePadX * 2.0f;
    const float badgeHeight = badgeTextSize.y + badgePadY * 2.0f;
    const ImVec2 badgeMin(max.x - badgeWidth - 10.0f * m_graphZoom, pos.y + 10.0f * m_graphZoom);
    const ImVec2 badgeMax(badgeMin.x + badgeWidth, badgeMin.y + badgeHeight);
    dl->AddRectFilled(badgeMin, badgeMax, StatusColor(status), 4.0f * m_graphZoom);
    DrawSafeTextAtFontSize(dl, ImVec2(badgeMin.x + badgePadX, badgeMin.y + badgePadY), IM_COL32(18, 20, 24, 255), statusText, badgeFontSize);

    if (node.id == m_asset.startNodeId) {
        DrawSafeTextScaled(dl, ImVec2(pos.x + 12.0f * m_graphZoom, max.y - 24.0f * m_graphZoom), IM_COL32(110, 175, 255, 255), "START", m_graphZoom);
    }

    const ImVec2 mousePos = ImGui::GetIO().MousePos;
    const bool bodyHovered = ImGui::IsMouseHoveringRect(pos, max, true);
    const bool outputPinHovered = PointInCircle(mousePos, outputPin, pin + 8.0f);
    const bool nodeHovered = bodyHovered || outputPinHovered;

    if (nodeHovered) {
        m_hoveredNodeId = node.id;
    }

    if (outputPinHovered) {
        dl->AddCircle(outputPin, pin + 3.0f, IM_COL32(180, 215, 255, 255), 18, 2.0f);
    }

    
    
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushID(static_cast<int>(node.id));
    ImGui::InvisibleButton("NodeBody", ImVec2(w, h), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    std::string dropped;
    if (m_scenePicker.AcceptSceneAssetDragDrop(dropped)) {
        ReplaceNodeScene(node.id, dropped);
    }

    if (outputPinHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        SelectNode(node.id);
        m_connecting = true;
        m_connectFromNodeId = node.id;
        m_connectionDragged = false;
        g_draggingGameLoopNodeId = 0;
    } else if (bodyHovered && ImGui::GetIO().KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        SelectNode(node.id);
        m_connecting = true;
        m_connectFromNodeId = node.id;
        m_connectionDragged = false;
        g_draggingGameLoopNodeId = 0;
    } else if (bodyHovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::IsKeyDown(ImGuiKey_Space) &&
        !outputPinHovered) {
        SelectNode(node.id);
        g_draggingGameLoopNodeId = node.id;
    }

    if (g_draggingGameLoopNodeId == node.id &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        !m_connecting &&
        !ImGui::GetIO().KeyAlt) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (std::fabs(delta.x) > 0.0001f || std::fabs(delta.y) > 0.0001f) {
            gp.x += delta.x / m_graphZoom;
            gp.y += delta.y / m_graphZoom;
            m_dirty = true;
        }
    }

    if (bodyHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        OpenPickerForReplace(node.id);
        g_draggingGameLoopNodeId = 0;
    }

    if (bodyHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        SelectNode(node.id);
        ImGui::OpenPopup("NodeContext");
    }

    DrawNodeContextMenu(node);
    ImGui::PopID();
}


void GameLoopEditorPanelInternal::DrawTransition(int index, const ImVec2& origin)
{
    GameLoopTransition& t = m_asset.transitions[index];
    NodeView* fv = FindView(t.fromNodeId);
    NodeView* tv = FindView(t.toNodeId);
    if (!fv || !tv) {
        return;
    }

    float w = NodeW * m_graphZoom;
    float h = NodeH * m_graphZoom;

    ImVec2 fp = GraphToScreen(fv->pos, origin);
    ImVec2 tp = GraphToScreen(tv->pos, origin);

    ImVec2 a(fp.x + w, fp.y + h * 0.5f);
    ImVec2 b(tp.x, tp.y + h * 0.5f);

    float dir = b.x >= a.x ? 1.0f : -1.0f;
    float tangent = 90.0f * m_graphZoom;
    ImVec2 c1(a.x + tangent * dir, a.y);
    ImVec2 c2(b.x - tangent * dir, b.y);

    bool hovered = NearBezier(ImGui::GetIO().MousePos, a, c1, c2, b, 12.0f * ClampF(m_graphZoom, 0.8f, 1.4f));
    bool selected = m_selection == SelectionKind::Transition && m_selectedTransitionIndex == index;
    if (hovered) {
        m_hoveredTransitionIndex = index;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 col = selected ? IM_COL32(120, 190, 255, 255) : (hovered ? IM_COL32(150, 170, 205, 255) : IM_COL32(105, 115, 135, 220));

    dl->AddBezierCubic(a, c1, c2, b, col, selected ? 3.5f : 2.2f);
    DrawArrow(dl, b, Bezier(a, c1, c2, b, 0.92f), col, ClampF(m_graphZoom, 0.75f, 1.4f));

    if (m_graphZoom >= 0.45f) {
        ImVec2 mid = Bezier(a, c1, c2, b, 0.5f);
        std::string label = TransitionLabel(t);
        const float labelFontSize = CalcSafeGraphTextFontSize(m_graphZoom);
        ImVec2 ts = CalcSafeTextSizeAtFontSize(label.c_str(), labelFontSize);
        float padX = 7.0f * ClampF(m_graphZoom, 0.8f, 1.2f);
        float padY = 3.0f * ClampF(m_graphZoom, 0.8f, 1.2f);
        ImVec2 r0(mid.x - ts.x * 0.5f - padX, mid.y - ts.y * 0.5f - padY);
        ImVec2 r1(mid.x + ts.x * 0.5f + padX, mid.y + ts.y * 0.5f + padY);
        dl->AddRectFilled(r0, r1, IM_COL32(30, 34, 42, 230), 5.0f * ClampF(m_graphZoom, 0.8f, 1.2f));
        dl->AddRect(r0, r1, col, 5.0f * ClampF(m_graphZoom, 0.8f, 1.2f));
        DrawSafeTextAtFontSize(dl, ImVec2(r0.x + padX, r0.y + padY), IM_COL32(235, 240, 250, 255), label.c_str(), labelFontSize);
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        SelectTransition(index);
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        SelectTransition(index);
        ImGui::OpenPopup("TransitionContext");
    }
    if (selected) {
        DrawTransitionContextMenu(index);
    }
}


void GameLoopEditorPanelInternal::DrawGraphContextMenu()
{
    if (ImGui::BeginPopup("GraphContext")) {
        if (ImGui::MenuItem("Add Scene Node")) {
            OpenPickerForCreate(m_contextGraphPos);
        }
        ImGui::EndPopup();
    }
}


void GameLoopEditorPanelInternal::DrawNodeContextMenu(GameLoopNode& node)
{
    if (ImGui::BeginPopup("NodeContext")) {
        if (ImGui::MenuItem("Replace Scene")) {
            OpenPickerForReplace(node.id);
        }
        if (ImGui::MenuItem("Set Start")) {
            m_asset.startNodeId = node.id;
            m_dirty = true;
        }
        if (ImGui::MenuItem("Delete")) {
            DeleteNode(node.id);
        }
        ImGui::EndPopup();
    }
}


void GameLoopEditorPanelInternal::DrawTransitionContextMenu(int index)
{
    if (ImGui::BeginPopup("TransitionContext")) {
        if (ImGui::MenuItem("Reverse")) {
            ReverseTransition(index);
        }
        if (ImGui::MenuItem("Delete")) {
            DeleteTransition(index);
        }
        ImGui::EndPopup();
    }
}


void GameLoopEditorPanelInternal::FitGraph(const ImVec2& size)
{
    if (m_asset.nodes.empty()) {
        m_graphOffset = { size.x * 0.5f, size.y * 0.5f };
        m_graphZoom = 1.0f;
        return;
    }

    for (auto& n : m_asset.nodes) {
        GetOrCreateNodePos(n.id);
    }

    float minX = m_nodeViews.front().pos.x;
    float minY = m_nodeViews.front().pos.y;
    float maxX = minX + NodeW;
    float maxY = minY + NodeH;

    for (const auto& v : m_nodeViews) {
        minX = MinF(minX, v.pos.x);
        minY = MinF(minY, v.pos.y);
        maxX = MaxF(maxX, v.pos.x + NodeW);
        maxY = MaxF(maxY, v.pos.y + NodeH);
    }

    float graphW = MaxF(maxX - minX, 1.0f);
    float graphH = MaxF(maxY - minY, 1.0f);
    float zoomX = (size.x - 80.0f) / graphW;
    float zoomY = (size.y - 80.0f) / graphH;

    m_graphZoom = ClampF(MinF(zoomX, zoomY), 0.4f, 1.2f);
    m_graphOffset.x = (size.x - graphW * m_graphZoom) * 0.5f - minX * m_graphZoom;
    m_graphOffset.y = (size.y - graphH * m_graphZoom) * 0.5f - minY * m_graphZoom;
}


ImVec2 GameLoopEditorPanelInternal::GraphToScreen(const DirectX::XMFLOAT2& p, const ImVec2& origin) const
{
    return ImVec2(
        origin.x + m_graphOffset.x + p.x * m_graphZoom,
        origin.y + m_graphOffset.y + p.y * m_graphZoom);
}


DirectX::XMFLOAT2 GameLoopEditorPanelInternal::ScreenToGraph(const ImVec2& p, const ImVec2& origin) const
{
    return {
        (p.x - origin.x - m_graphOffset.x) / m_graphZoom,
        (p.y - origin.y - m_graphOffset.y) / m_graphZoom
    };
}
