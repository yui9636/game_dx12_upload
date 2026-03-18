#include "CinematicPlayComponent.h"
#include "Actor/Actor.h"        // GetName()のために必要
#include "Cinematic/CinematicTrack.h" // TransformTrackのために必要
#include "imgui.h"
#include "System/Dialog.h"

using namespace Cinematic;

void CinematicPlayComponent::Start()
{
    if (!sequenceFilePath.empty())
    {
        sequence = std::make_shared<Sequence>();

        // ★既存のSequenceクラスにLoadFromFileが追加されている前提
        sequence->LoadFromFile(sequenceFilePath);

        // -------------------------------------------------------
        // ★バインド処理: 名前から実体を探してリンクする
        // -------------------------------------------------------
        const auto& actors = ActorManager::Instance().GetActors();

        for (auto& track : sequence->tracks)
        {
            
            // CameraTrackの場合: カメラコントローラーをバインド
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
            Stop(); // ループさせたい場合はここで currentTime = 0;
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

// 保存
void CinematicPlayComponent::Serialize(json& outJson) const
{
    outJson["file"] = sequenceFilePath;
    outJson["playOnStart"] = playOnStart;
}

// 読み込み
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