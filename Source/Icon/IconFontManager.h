#pragma once
#include <imgui.h>
#include <vector>
#include <map>
#include <string>
#include "IconsFontAwesome7.h" //

enum class IconFontSize { Mini,Small, Medium, Large, Extra }; // 必要に応じて増やせる
enum class IconSemantic { Default, Success, Danger, Warning, Info };

class IconFontManager {
public:
    static IconFontManager& Instance() {
        static IconFontManager instance;
        return instance;
    }

    // サイズと用途を紐付ける設定構造体
    struct SizeConfig {
        IconFontSize type;
        float size;
    };

    // 柔軟性：外部から設定リストを流し込む。デフォルト値も設定可能
    void Setup(const std::vector<SizeConfig>& configs);

    // アイコン専用ボタン（文字は絶対に出さない）
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
        bool pushed = false; // ★自分がフォントを積んだかどうかの旗

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