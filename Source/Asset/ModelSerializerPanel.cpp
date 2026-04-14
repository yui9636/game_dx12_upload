#include "ModelSerializerPanel.h"

#include "Engine/EditorSelection.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

namespace
{
    constexpr size_t kPathBufferSize = 512;

    void CopyStringToBuffer(const std::string& value, std::array<char, kPathBufferSize>& buffer)
    {
        buffer.fill('\0');
        if (value.empty()) {
            return;
        }

        const size_t copyLength = (std::min)(value.size(), buffer.size() - 1);
        memcpy(buffer.data(), value.data(), copyLength);
        buffer[copyLength] = '\0';
    }
}

bool ModelSerializerPanel::AcceptSourceAsset(const std::filesystem::path& path)
{
    if (!IsSupportedSourceAsset(path)) {
        m_hasResult = true;
        m_lastResult = {};
        m_lastResult.message = "Source model only: .fbx / .obj / .blend / .gltf / .glb";
        return false;
    }

    m_sourcePath = path;
    m_outputPath = BuildDefaultOutputPath(path).string();
    return true;
}

bool ModelSerializerPanel::IsSupportedSourceAsset(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    return extension == ".fbx" || extension == ".obj" || extension == ".blend" || extension == ".gltf" || extension == ".glb";
}

std::filesystem::path ModelSerializerPanel::BuildDefaultOutputPath(const std::filesystem::path& sourcePath)
{
    std::filesystem::path outputPath = sourcePath;
    outputPath.replace_extension(".cereal");
    return outputPath;
}

void ModelSerializerPanel::Draw(bool* p_open, bool* outFocused)
{
    if (!ImGui::Begin("Serializer", p_open)) {
        if (outFocused) {
            *outFocused = false;
        }
        ImGui::End();
        return;
    }

    if (outFocused) {
        *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

    ImGui::TextWrapped("Drop a source model from the Asset Browser and build a manual .cereal serializer.");

    if (ImGui::Button("Use Selected Asset")) {
        const EditorSelection& selection = EditorSelection::Instance();
        if (selection.GetType() == SelectionType::Asset) {
            AcceptSourceAsset(selection.GetAssetPath());
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        m_sourcePath.clear();
        m_outputPath.clear();
    }

    ImGui::Separator();

    std::array<char, kPathBufferSize> sourceBuffer{};
    CopyStringToBuffer(m_sourcePath.string(), sourceBuffer);
    ImGui::InputText("Source", sourceBuffer.data(), sourceBuffer.size(), ImGuiInputTextFlags_ReadOnly);
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
            AcceptSourceAsset(static_cast<const char*>(payload->Data));
        }
        ImGui::EndDragDropTarget();
    }

    std::array<char, kPathBufferSize> outputBuffer{};
    CopyStringToBuffer(m_outputPath, outputBuffer);
    if (ImGui::InputText("Output", outputBuffer.data(), outputBuffer.size())) {
        m_outputPath = outputBuffer.data();
    }

    ImGui::Spacing();
    ImGui::Checkbox("Simplify static meshes", &m_settings.enableSimplification);
    if (m_settings.enableSimplification) {
        ImGui::SliderFloat("Triangle Ratio", &m_settings.targetTriangleRatio, 0.05f, 1.0f, "%.2f");
        ImGui::DragFloat("Target Error", &m_settings.targetError, 0.001f, 0.0f, 1.0f, "%.3f");
        ImGui::Checkbox("Lock Border", &m_settings.lockBorder);
    }
    ImGui::Checkbox("Optimize Vertex Cache", &m_settings.optimizeVertexCache);
    ImGui::Checkbox("Optimize Overdraw", &m_settings.optimizeOverdraw);
    if (m_settings.optimizeOverdraw) {
        ImGui::SliderFloat("Overdraw Threshold", &m_settings.overdrawThreshold, 1.0f, 3.0f, "%.2f");
    }
    ImGui::Checkbox("Optimize Vertex Fetch", &m_settings.optimizeVertexFetch);
    ImGui::DragFloat("Scaling", &m_settings.scaling, 0.01f, 0.001f, 100.0f, "%.3f");

    ImGui::Spacing();
    ImGui::TextDisabled("Skinned mesh simplification is skipped automatically for safety.");

    const bool canBuild = !m_sourcePath.empty() && IsSupportedSourceAsset(m_sourcePath);
    if (!canBuild) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Build Serializer", ImVec2(180.0f, 0.0f))) {
        m_lastResult = ModelAssetSerializer::Build(m_sourcePath.string(), m_outputPath, m_settings);
        m_hasResult = true;
    }
    if (!canBuild) {
        ImGui::EndDisabled();
    }

    if (m_hasResult) {
        ImGui::Separator();
        const ImVec4 color = m_lastResult.success
            ? ImVec4(0.45f, 0.95f, 0.55f, 1.0f)
            : ImVec4(1.0f, 0.45f, 0.45f, 1.0f);
        ImGui::TextColored(color, "%s", m_lastResult.message.c_str());
        if (!m_lastResult.outputPath.empty()) {
            ImGui::TextWrapped("Output: %s", m_lastResult.outputPath.c_str());
        }
        ImGui::Text("Meshes: %zu", m_lastResult.processedMeshCount);
        ImGui::Text("Simplified: %zu", m_lastResult.simplifiedMeshCount);
        ImGui::Text("Skipped(skin): %zu", m_lastResult.skippedSimplificationMeshCount);
        ImGui::Text("Indices: %zu -> %zu", m_lastResult.sourceIndexCount, m_lastResult.outputIndexCount);
        ImGui::Text("Vertices: %zu -> %zu", m_lastResult.sourceVertexCount, m_lastResult.outputVertexCount);
    }

    ImGui::End();
}
