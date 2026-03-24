#include "CinematicPlayComponent.h"
#include "Actor/Actor.h"
#include "Cinematic/CinematicTrack.h"
#include "imgui.h"
#include "System/Dialog.h"

using namespace Cinematic;

void CinematicPlayComponent::Start()
{
    if (!sequenceFilePath.empty())
    {
        sequence = std::make_shared<Sequence>();

        sequence->LoadFromFile(sequenceFilePath);

        // -------------------------------------------------------
        // -------------------------------------------------------
        const auto& actors = ActorManager::Instance().GetActors();

        for (auto& track : sequence->tracks)
        {
            
            if (auto camTrack = std::dynamic_pointer_cast<CameraTrack>(track))
            {
                camTrack->Bind(CameraController::Instance());
            }
        }

        if (playOnStart)
        {
            Play();
        }
    }
}

void CinematicPlayComponent::Update(float dt)
{
    if (isPlaying && sequence)
    {
        currentTime += dt;
        if (currentTime >= sequence->duration)
        {
            currentTime = sequence->duration;
            Stop();
        }
        sequence->Evaluate(currentTime);
    }
}

void CinematicPlayComponent::Play()
{
    if (sequence)
    {
        isPlaying = true;
        currentTime = 0.0f;
    }
}

void CinematicPlayComponent::Stop()
{
    isPlaying = false;
    currentTime = 0.0f;
}

void CinematicPlayComponent::Serialize(json& outJson) const
{
    outJson["file"] = sequenceFilePath;
    outJson["playOnStart"] = playOnStart;
}

void CinematicPlayComponent::Deserialize(const json& inJson)
{
    if (inJson.contains("file")) inJson.at("file").get_to(sequenceFilePath);
    if (inJson.contains("playOnStart")) inJson.at("playOnStart").get_to(playOnStart);
}

// GUI
void CinematicPlayComponent::OnGUI()
{
    ImGui::Text("Sequence File:");
    char buf[256]; strcpy_s(buf, sequenceFilePath.c_str());
    if (ImGui::InputText("##File", buf, sizeof(buf))) sequenceFilePath = buf;

    ImGui::SameLine();
    if (ImGui::Button("...")) {
        char path[MAX_PATH] = "";
        if (Dialog::OpenFileName(path, MAX_PATH, "JSON Files\0*.json\0All Files\0*.*\0") == DialogResult::OK)
        {
            sequenceFilePath = path;
        }
    }

    ImGui::Checkbox("Play On Start", &playOnStart);

    if (ImGui::Button("Test Play")) Play();
    ImGui::SameLine();
    if (ImGui::Button("Stop")) Stop();
}
