#pragma once
#include <imgui.h>
#include <vector>
#include <map>
#include <string>
#include "IconsFontAwesome7.h" //

enum class IconFontSize { Mini,Small, Medium, Large, Extra };
enum class IconSemantic { Default, Success, Danger, Warning, Info };

class IconFontManager {
public:
    static IconFontManager& Instance() {
        static IconFontManager instance;
        return instance;
    }

    struct SizeConfig {
        IconFontSize type;
        float size;
    };

    void Setup(const std::vector<SizeConfig>& configs);

    bool IconButton(const char* icon,
        IconSemantic semantic = IconSemantic::Default,
        IconFontSize size = IconFontSize::Medium,
        const char* tooltip = nullptr);

    ImFont* GetFontInternal(IconFontSize size) const;

private:
    IconFontManager() = default;
    std::map<IconFontSize, ImFont*> fontMap;

    ImVec4 GetSemanticColor(IconSemantic semantic) const;

    struct ScopedFont {
        bool pushed = false;

        ScopedFont(ImFont* font) {
            if (font) {
                ImGui::PushFont(font);
                pushed = true;
            }
        }

        ~ScopedFont() {
            if (pushed) {
                ImGui::PopFont();
            }
        }
    };


};
