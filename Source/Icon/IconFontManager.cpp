#include "IconFontManager.h"

void IconFontManager::Setup(const std::vector<SizeConfig>& configs)
{
    ImGuiIO& io = ImGui::GetIO();

    const std::string iconFontPath = "Data/Font/Font Awesome 7 Free-Solid-900.otf";
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

    fontMap.clear();

    for (const auto& conf : configs)
    {
        ImFontConfig config;
        config.PixelSnapH = true;
        config.MergeMode = true;

        ImFont* font = io.Fonts->AddFontFromFileTTF(iconFontPath.c_str(), conf.size, &config, icons_ranges);

        if (font == nullptr) {
            printf("CRITICAL ERROR: Failed to load font file at %s\n", iconFontPath.c_str());
            continue;
        }

     
        fontMap[conf.type] = font;
    }
}

bool IconFontManager::IconButton(const char* icon, IconSemantic semantic, IconFontSize size, const char* tooltip)
{
    ScopedFont fontScope(GetFontInternal(size));

    ImGui::PushStyleColor(ImGuiCol_Text, GetSemanticColor(semantic));
    bool clicked = ImGui::Button(icon);
    ImGui::PopStyleColor();

    if (tooltip && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", tooltip); // ツールチップは標準フォントで文字が出る
    }

    return clicked;
}

ImFont* IconFontManager::GetFontInternal(IconFontSize size) const
{
    auto it = fontMap.find(size);
    return (it != fontMap.end()) ? it->second : nullptr;
}

ImVec4 IconFontManager::GetSemanticColor(IconSemantic semantic) const
{
    switch (semantic)
    {
    case IconSemantic::Success: return ImVec4(0.26f, 0.90f, 0.26f, 1.00f); // Play (緑)
    case IconSemantic::Danger:  return ImVec4(1.00f, 0.25f, 0.25f, 1.00f); // Stop (赤)
    case IconSemantic::Warning: return ImVec4(1.00f, 0.60f, 0.00f, 1.00f); // Pause (橙)
    case IconSemantic::Info:    return ImVec4(0.20f, 0.60f, 1.00f, 1.00f); // Info (青)
    default:                   return ImGui::GetStyle().Colors[ImGuiCol_Text];
    }
}
