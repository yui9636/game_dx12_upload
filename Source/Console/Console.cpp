#include "Console.h"
#include "Logger.h"
#include "Profiler.h"
#include <imgui.h>
#include <string>

Console& Console::Instance() {
    static Console instance;
    return instance;
}

void Console::Draw(const char* title, bool* p_open, bool* outFocused) {
    if (!ImGui::Begin(title, p_open)) {
        if (outFocused) {
            *outFocused = false;
        }
        ImGui::End();
        Profiler::Instance().Clear();
        return;
    }

    if (outFocused) {
        *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

    if (ImGui::BeginTabBar("ConsoleTabs")) {

        // =========================================================
        // =========================================================
        if (ImGui::BeginTabItem("Profiler")) {

            float totalPassTime = 0.0f;
            const auto& results = Profiler::Instance().GetResults();

            for (const auto& res : results) {
                float fraction = res.timeMs / 16.0f;

                if (res.timeMs > 2.0f) ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.4f, 0.0f, 1.0f));
                else ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));

                ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), (res.name + ": " + std::to_string(res.timeMs) + " ms").c_str());
                ImGui::PopStyleColor();

                if (res.name != "Total RenderPipeline") totalPassTime += res.timeMs;
            }

            ImGui::Separator();
            ImGui::Text("Passes CPU Total: %.3f ms", totalPassTime);

            ImGui::EndTabItem();
        }

        // =========================================================
        // =========================================================
        if (ImGui::BeginTabItem("Logs")) {
            if (ImGui::Button("Clear Logs")) {
                Logger::Instance().ClearLogs();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll", &m_autoScroll);
            ImGui::Separator();

            ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

            const auto& logs = Logger::Instance().GetLogs();
            for (const auto& log : logs) {
                if (log.level == LogLevel::Error) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                else if (log.level == LogLevel::Warning) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));

                ImGui::TextUnformatted(log.message.c_str());

                if (log.level != LogLevel::Info) ImGui::PopStyleColor();
            }

            if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();

    Profiler::Instance().Clear();
}
