#include "BGMComponent.h"
#include "Audio/Audio.h"
#include "Audio/AudioSource.h"
#include "JSONManager.h"
#include "imgui.h"
#include "System/Dialog.h"

void BGMComponent::Start()
{
    if (!filePath.empty())
    {
        if (playingSource) playingSource->Stop();

        playingSource = Audio::Instance()->Play2D(filePath, volume, pitch, loop);
    }
}

void BGMComponent::OnDestroy()
{
    StopPreview();
}

void BGMComponent::OnGUI()
{
    ImGui::TextDisabled("Background Music");

    char buf[256];
    strcpy_s(buf, filePath.c_str());
    if (ImGui::InputText("File", buf, sizeof(buf))) {
        filePath = buf;
    }
    ImGui::SameLine();
    if (ImGui::Button("..."))
    {
        char path[MAX_PATH] = "";
        if (Dialog::OpenFileName(path, MAX_PATH, "Audio Files\0*.wav\0All Files\0*.*\0") == DialogResult::OK)
        {
            filePath = path;
        }
    }

    bool changed = false;
    changed |= ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f);
    changed |= ImGui::SliderFloat("Pitch", &pitch, 0.1f, 2.0f);
    changed |= ImGui::Checkbox("Loop", &loop);

    if (changed && playingSource && playingSource->IsPlaying())
    {
        playingSource->SetVolume(volume);
        playingSource->SetPitch(pitch);
        playingSource->SetLoop(loop);
    }

    ImGui::Separator();
    float width = ImGui::GetContentRegionAvail().x;

    if (ImGui::Button("Play Preview", ImVec2(width * 0.5f, 0)))
    {
        PlayPreview();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop", ImVec2(width * 0.5f, 0)))
    {
        StopPreview();
    }
}

void BGMComponent::PlayPreview()
{
    StopPreview();
    if (!filePath.empty())
    {
        playingSource = Audio::Instance()->Play2D(filePath, volume, pitch, loop);
    }
}

void BGMComponent::StopPreview()
{
    if (playingSource)
    {
        playingSource->Stop();
        playingSource.reset();
    }
}

void BGMComponent::Serialize(json& outJson) const
{
    outJson["filePath"] = filePath;
    outJson["volume"] = volume;
    outJson["pitch"] = pitch;
    outJson["loop"] = loop;
}

void BGMComponent::Deserialize(const json& inJson)
{
    filePath = inJson.value("filePath", "");
    volume = inJson.value("volume", 0.5f);
    pitch = inJson.value("pitch", 1.0f);
    loop = inJson.value("loop", true);
}
