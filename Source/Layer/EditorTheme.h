#pragma once

#include <imgui.h>
// 変更を適用する。EditorGrayTheme は計算済みの結果を対象オブジェクトへ反映する。

inline void ApplyEditorGrayTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    const ImVec4 gray_100 = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    const ImVec4 gray_070 = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    const ImVec4 gray_050 = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    const ImVec4 gray_030 = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    const ImVec4 unity_blue = ImVec4(0.17f, 0.36f, 0.53f, 1.00f);

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

// UE5 Slate Starship Dark テーマ (FStyleColors 由来):
//   Background #1A1A1A  Title #151515  Header #1F1F1F  Panel #242424
//   Recessed #0F0F0F  Foldout #2C2C2C  Input #0A0A0A  Outline #383838
//   Hover #575757  Select #0070E0 (公式青)  Primary #0070E0  Foreground #C8C8C8
inline void PushPlayerEditorPanelStyle()
{
    const ImVec4 ueBackground = ImVec4(0.102f, 0.102f, 0.102f, 1.0f);
    const ImVec4 ueTitle      = ImVec4(0.082f, 0.082f, 0.082f, 1.0f);
    const ImVec4 ueHeader     = ImVec4(0.122f, 0.122f, 0.122f, 1.0f);
    const ImVec4 uePanel      = ImVec4(0.141f, 0.141f, 0.141f, 1.0f);
    const ImVec4 ueRecessed   = ImVec4(0.059f, 0.059f, 0.059f, 1.0f);
    const ImVec4 ueFoldout    = ImVec4(0.173f, 0.173f, 0.173f, 1.0f);
    const ImVec4 ueInput      = ImVec4(0.039f, 0.039f, 0.039f, 1.0f);
    const ImVec4 ueOutline    = ImVec4(0.220f, 0.220f, 0.220f, 1.0f);
    const ImVec4 ueHover      = ImVec4(0.341f, 0.341f, 0.341f, 1.0f);
    const ImVec4 ueDropdown   = ImVec4(0.220f, 0.220f, 0.220f, 1.0f);
    const ImVec4 ueSelect     = ImVec4(0.000f, 0.439f, 0.878f, 1.0f);
    const ImVec4 ueSelectHv   = ImVec4(0.055f, 0.525f, 1.000f, 1.0f);
    const ImVec4 ueSelectPr   = ImVec4(0.000f, 0.361f, 0.753f, 1.0f);
    const ImVec4 uePrimary    = ImVec4(0.000f, 0.439f, 0.878f, 1.0f);
    const ImVec4 ueForeground = ImVec4(0.784f, 0.784f, 0.784f, 1.0f);
    const ImVec4 ueForeDim    = ImVec4(0.500f, 0.500f, 0.500f, 1.0f);
    const ImVec4 ueBorder     = ImVec4(0.059f, 0.059f, 0.059f, 1.0f);
    const ImVec4 ueSeparator  = ImVec4(0.220f, 0.220f, 0.220f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg,           uePanel);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,            uePanel);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,            ueDropdown);
    ImGui::PushStyleColor(ImGuiCol_TitleBg,            ueTitle);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,      ueTitle);
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed,   ueBackground);
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg,          ueHeader);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,            ueInput);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,     ueOutline);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,      ueSelect);
    ImGui::PushStyleColor(ImGuiCol_Tab,                ueHeader);
    ImGui::PushStyleColor(ImGuiCol_TabHovered,         ueFoldout);
    ImGui::PushStyleColor(ImGuiCol_TabActive,          uePanel);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocused,       ueHeader);
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, uePanel);
    ImGui::PushStyleColor(ImGuiCol_Border,             ueBorder);
    ImGui::PushStyleColor(ImGuiCol_Separator,          ueSeparator);
    ImGui::PushStyleColor(ImGuiCol_SeparatorHovered,   uePrimary);
    ImGui::PushStyleColor(ImGuiCol_SeparatorActive,    uePrimary);
    ImGui::PushStyleColor(ImGuiCol_Header,             ueSelect);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,      ueSelectHv);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,       ueSelectPr);
    ImGui::PushStyleColor(ImGuiCol_Button,             ueHeader);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,      ueFoldout);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,       ueOutline);
    ImGui::PushStyleColor(ImGuiCol_CheckMark,          uePrimary);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,         uePrimary);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,   ueSelectHv);
    ImGui::PushStyleColor(ImGuiCol_ResizeGrip,         ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered,  uePrimary);
    ImGui::PushStyleColor(ImGuiCol_ResizeGripActive,   ueSelect);
    ImGui::PushStyleColor(ImGuiCol_Text,               ueForeground);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,       ueForeDim);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg,     ueSelect);
    ImGui::PushStyleColor(ImGuiCol_DragDropTarget,     uePrimary);
    ImGui::PushStyleColor(ImGuiCol_NavHighlight,       uePrimary);
    ImGui::PushStyleColor(ImGuiCol_DockingPreview,     ImVec4(0.000f, 0.439f, 0.878f, 0.50f));
    ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg,     ueBackground);
}

inline void PopPlayerEditorPanelStyle()
{
    ImGui::PopStyleColor(38);
}

inline void PushPlayerEditorPanelSizeStyle()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,    0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,    2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding,      0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,     2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,     ImVec2(6.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,      ImVec2(4.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(4.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize,  1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,  0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_TabBorderSize,    0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,    10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding,0.0f);
}

inline void PopPlayerEditorPanelSizeStyle()
{
    ImGui::PopStyleVar(15);
}

