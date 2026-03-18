#include "EditorLayer.h"
#include "Engine/EngineKernel.h" // カーネルの機能(Play/Stop)を呼ぶため
#include "Icon/IconFontManager.h"
#include "Engine/EditorSelection.h" // 選択状態の管理
#include "Inspector/InspectorECSUI.h" // インスペクターUI
#include "Component/NameComponent.h"  // 名前表示用
#include "Archetype/Archetype.h"      // エンティティ走査用
#include <imgui.h>
#include <imgui_internal.h>
#include "Hierarchy/HierarchyECSUI.h"
#include <Component\CameraBehaviorComponent.h>
#include "Generated/ComponentMeta.generated.h"
#include "Component/EnvironmentComponent.h"
#include "Component/PostEffectComponent.h"
#include "Console/Console.h"

namespace {
    template <typename T>
    bool DrawSettingWidget(std::string_view name, T& value) {
        bool changed = false;
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%s", name.data());
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1);

        std::string id = std::string("##") + name.data();

        if constexpr (std::is_same_v<T, float>) { changed = ImGui::DragFloat(id.c_str(), &value, 0.01f); }
        else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, uint32_t>) { changed = ImGui::DragInt(id.c_str(), (int*)&value, 1); }
        else if constexpr (std::is_same_v<T, bool>) { changed = ImGui::Checkbox(id.c_str(), &value); }
        else if constexpr (std::is_same_v<T, std::string>) {
            char buf[256]; strcpy_s(buf, value.c_str());
            if (ImGui::InputText(id.c_str(), buf, 256)) { value = buf; changed = true; }

            // ★ 文字列項目なら、自動的にアセットのドラッグ＆ドロップを受け付ける！
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
                    value = (const char*)payload->Data;
                    changed = true;
                }
                ImGui::EndDragDropTarget();
            }
        }
        else if constexpr (std::is_same_v<T, DirectX::XMFLOAT3>) { changed = ImGui::ColorEdit3(id.c_str(), &value.x); } // 3要素は色と仮定
        else if constexpr (std::is_same_v<T, DirectX::XMFLOAT4>) { changed = ImGui::ColorEdit4(id.c_str(), &value.x); }
        else if constexpr (std::is_enum_v<T>) {
            int enumVal = static_cast<int>(value);
            if (ImGui::DragInt(id.c_str(), &enumVal, 1)) { value = static_cast<T>(enumVal); changed = true; }
        }
        else { ImGui::TextDisabled("Unsupported Type"); }
        return changed;
    }

    // 構造体の全メンバ変数を展開してテーブル化する関数
    template <typename T>
    void DrawGlobalSettingsUI(T& comp) {
        constexpr auto& metaName = ComponentMeta<T>::Name;

        ImGui::PushID(metaName.data());
        if (ImGui::CollapsingHeader(metaName.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("CompTable", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

                std::apply([&](auto... fields) {
                    (DrawSettingWidget(fields.name, comp.*(fields.ptr)), ...);
                    }, ComponentMeta<T>::Fields);

                ImGui::EndTable();
            }
        }
        ImGui::PopID();
    }
}
void ApplyUnityTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    // --- Unity 2022 Dark Theme Color Palette ---
    const ImVec4 gray_100 = ImVec4(0.82f, 0.82f, 0.82f, 1.00f); // Text
    const ImVec4 gray_070 = ImVec4(0.24f, 0.24f, 0.24f, 1.00f); // Header / Active
    const ImVec4 gray_050 = ImVec4(0.22f, 0.22f, 0.22f, 1.00f); // Background
    const ImVec4 gray_030 = ImVec4(0.18f, 0.18f, 0.18f, 1.00f); // Input / Darker
    const ImVec4 unity_blue = ImVec4(0.17f, 0.36f, 0.53f, 1.00f); // Selection

    colors[ImGuiCol_Text] = gray_100;
    colors[ImGuiCol_WindowBg] = gray_050;
    colors[ImGuiCol_ChildBg] = gray_050;
    colors[ImGuiCol_PopupBg] = gray_030;
    colors[ImGuiCol_Border] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBg] = gray_030;
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_TitleBg] = gray_070;
    colors[ImGuiCol_TitleBgActive] = gray_070;
    colors[ImGuiCol_MenuBarBg] = gray_070;
    colors[ImGuiCol_Header] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderActive] = unity_blue;
    colors[ImGuiCol_Button] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive] = unity_blue;
    colors[ImGuiCol_Tab] = gray_050;
    colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_TabActive] = gray_070;
    colors[ImGuiCol_TabUnfocused] = gray_050;
    colors[ImGuiCol_TabUnfocusedActive] = gray_070;
    colors[ImGuiCol_DockingPreview] = ImVec4(0.17f, 0.36f, 0.53f, 0.70f);
}

EditorLayer::EditorLayer(GameLayer* gameLayer)
    : m_gameLayer(gameLayer)
{
}

void EditorLayer::Initialize()
{

    m_assetBrowser = std::make_unique<AssetBrowser>();
    m_assetBrowser->Initialize();

    ApplyUnityTheme();
}

void EditorLayer::Finalize()
{
}

void EditorLayer::Update(const EngineTime& time)
{
}

void EditorLayer::RenderUI()
{
    // カーネルから移動してきた大枠のUI描画
    DrawDockSpace();
    DrawMenuBar();
    DrawMainToolbar();

    // 各種パネル
    DrawSceneView();
    DrawHierarchy();
    DrawInspector();

    DrawLightingWindow();
    DrawGBufferDebugWindow();
    Console::Instance().Draw();
    if (m_assetBrowser) m_assetBrowser->RenderUI();
}

void EditorLayer::DrawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File(F)")) { ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Edit(E)")) { ImGui::EndMenu(); }
        if (ImGui::BeginMenu("View(V)")) { ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Game(G)")) { ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Window(W)")) {
            if (ImGui::MenuItem(ICON_FA_SUN " Lighting Settings")) {
                m_showLightingWindow = true;
            }
            if (ImGui::MenuItem(ICON_FA_IMAGES " G-Buffer Debug")) {


                m_showGBufferDebug = true;
            }

            ImGui::EndMenu();
        }

        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200.0f);
        ImGui::TextDisabled(ICON_FA_GEAR " %.1f FPS", ImGui::GetIO().Framerate);
        ImGui::SameLine();

        float totalTime = EngineKernel::Instance().GetTime().totalTime;
        ImGui::TextDisabled(ICON_FA_CLOCK " %.2f", totalTime);

        ImGui::EndMainMenuBar();
    }
}

void EditorLayer::DrawMainToolbar()
{
    auto& ifm = IconFontManager::Instance();
    ImGuiViewport* vp = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + ImGui::GetFrameHeight()));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, 32));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

    if (ImGui::Begin("##MainToolbar", nullptr, flags))
    {
        // 左側: ファイル操作
        if (ifm.IconButton(ICON_FA_FLOPPY_DISK, IconSemantic::Default, IconFontSize::Medium, nullptr)) { /* Save */ }
        ImGui::SameLine();
        if (ifm.IconButton(ICON_FA_ARROW_ROTATE_LEFT, IconSemantic::Default, IconFontSize::Medium, nullptr)) { /* Undo */ }
        ImGui::SameLine();
        if (ifm.IconButton(ICON_FA_ARROW_ROTATE_RIGHT, IconSemantic::Default, IconFontSize::Medium, nullptr)) { /* Redo */ }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // 中央: 再生コントロール
        float centerX = ImGui::GetWindowWidth() * 0.5f;
        ImGui::SetCursorPosX(centerX - 45.0f);

        // ★現在のモードをカーネルから取得
        EngineMode mode = EngineKernel::Instance().GetMode();

        // [Play]
        bool isPlaying = (mode == EngineMode::Play);
        ImVec4 playColor = isPlaying ? ImVec4(0.26f, 0.90f, 0.26f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, playColor);
        if (ifm.IconButton(ICON_FA_PLAY, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            // ★カーネルの関数を直接叩く
            if (mode == EngineMode::Editor || mode == EngineMode::Pause) EngineKernel::Instance().Play();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // [Pause]
        bool isPaused = (mode == EngineMode::Pause);
        ImVec4 pauseColor = isPaused ? ImVec4(1.00f, 0.60f, 0.00f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, pauseColor);
        if (ifm.IconButton(ICON_FA_PAUSE, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            // ★カーネルの関数を直接叩く
            if (mode == EngineMode::Play) EngineKernel::Instance().Pause();
            else if (mode == EngineMode::Pause) EngineKernel::Instance().Play();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // [Stop]
        bool canStop = (mode != EngineMode::Editor);
        ImVec4 stopColor = canStop ? ImVec4(1.00f, 0.25f, 0.25f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, stopColor);
        if (ifm.IconButton(ICON_FA_SQUARE, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            // ★カーネルの関数を直接叩く
            if (canStop) EngineKernel::Instance().Stop();
        }
        ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(2);
}

void EditorLayer::DrawDockSpace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    // 背景を描画しないフラグ（残像防止のため背景を塗る設定にします）
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    // ★ここでDockSpaceの土台ウィンドウを描画（これが残像を消すキャンバスになります）
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("EditorDockSpace");

    // 中央ノードのパススルー設定
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    static bool isLayoutInitialized = false;
    if (!isLayoutInitialized)
    {
        isLayoutInitialized = true;
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);

        // 画面を分割していく
        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
        ImGuiID dock_down = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

        // 各ウィンドウを分割したエリアにはめ込む
        ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow(ICON_FA_CIRCLE_INFO " Inspector", dock_right);

        ImGui::DockBuilderDockWindow(ICON_FA_FOLDER_OPEN " Asset Browser", dock_down);
        ImGui::DockBuilderDockWindow("Console", dock_down); // ★ 追加：コンソールを下のエリアに重ねる（タブ化）

        ImGui::DockBuilderDockWindow("Scene View", dock_main_id); // 中央

        ImGui::DockBuilderFinish(dockspace_id);
    }



    ImGui::End();
}

void EditorLayer::DrawSceneView()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("Scene View", nullptr, window_flags))
    {
        // =========================================================
        // ★ 判定の強化：子ウィンドウやアイテム（画像）の上でもホバーとみなす
        // =========================================================
        bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        // レジストリからカメラ制御コンポーネントを探してフラグを更新
        auto& registry = m_gameLayer->GetRegistry();
        auto archetypes = registry.GetAllArchetypes();
        for (auto* arch : archetypes) {
            // CameraFreeControlComponent を持つアーキタイプかチェック
            if (arch->GetSignature().test(TypeManager::GetComponentTypeID<CameraFreeControlComponent>())) {
                auto* ctrlCol = arch->GetColumn(TypeManager::GetComponentTypeID<CameraFreeControlComponent>());
                // アーキタイプ内の全カメラエンティティにホバー状態を伝える
                for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
                    static_cast<CameraFreeControlComponent*>(ctrlCol->Get(i))->isHovered = hovered;
                }
            }
        }

        // ホバー中に右クリックされたらフォーカスを奪う（WASD操作のため）
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::SetWindowFocus();
        }

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        Graphics& graphics = Graphics::Instance();
        FrameBufferId sceneViewBufferId =
            (graphics.GetAPI() == GraphicsAPI::DX12) ? FrameBufferId::Scene : FrameBufferId::Display;
        FrameBuffer* sceneViewBuffer = graphics.GetFrameBuffer(sceneViewBufferId);
        if (sceneViewBuffer) {
            if (void* sceneViewTexture = sceneViewBuffer->GetImGuiTextureID()) {
                // DX11 ?????? SRV?DX12 ? ImGui ?????????? SRV ??????
                ImGui::Image((ImTextureID)sceneViewTexture, viewportSize);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorLayer::DrawHierarchy()
{
    if (!m_gameLayer) return;

    HierarchyECSUI::Render(&m_gameLayer->GetRegistry());

}

void EditorLayer::DrawInspector()
{
    if (!m_gameLayer) return;  // InspectorECSUI::Render が Begin/End を管理
    InspectorECSUI::Render(&m_gameLayer->GetRegistry());
}

void EditorLayer::DrawLightingWindow()
{
    if (!m_showLightingWindow) return;

    // ウィンドウを描画
    if (ImGui::Begin(ICON_FA_SUN " Lighting Settings", &m_showLightingWindow))
    {
        // 1. Environment の自動生成UI
        auto& env = m_gameLayer->GetEnvironment();
        DrawGlobalSettingsUI(env);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 2. PostEffect の自動生成UI
        auto& post = m_gameLayer->GetPostEffect();
        DrawGlobalSettingsUI(post);
    }
    ImGui::End();
}

void EditorLayer::DrawGBufferDebugWindow()
{
    if (!m_showGBufferDebug) return;

    if (ImGui::Begin(ICON_FA_IMAGES " G-Buffer Debug", &m_showGBufferDebug))
    {
        // GBufferを取得
        FrameBuffer* gbuffer = Graphics::Instance().GetFrameBuffer(FrameBufferId::GBuffer);
        if (gbuffer)
        {
            // ウィンドウ幅に合わせてアスペクト比を計算
            float w = ImGui::GetContentRegionAvail().x;
            float h = w * (Graphics::Instance().GetScreenHeight() / Graphics::Instance().GetScreenWidth());
            ImVec2 size(w, h);

            ImGui::TextDisabled("Target 0: Albedo (RGB) + Metallic (A)");
            if (void* tex0 = gbuffer->GetImGuiTextureID(0)) {
                ImGui::Image((ImTextureID)tex0, size);
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Target 1: Normal (RGB) + Roughness (A)");
            if (void* tex1 = gbuffer->GetImGuiTextureID(1)) {
                ImGui::Image((ImTextureID)tex1, size);
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Target 2: World Position (RGB) + Depth (A)");
            if (void* tex2 = gbuffer->GetImGuiTextureID(2)) {
                ImGui::Image((ImTextureID)tex2, size);
            }
            ImGui::Spacing();
            ImGui::TextDisabled("Target 3: Velocity (RG)");
            if (void* tex3 = gbuffer->GetImGuiTextureID(3)) {
                ImGui::Image((ImTextureID)tex3, size);
            }

        }
    }
    ImGui::End();
}

