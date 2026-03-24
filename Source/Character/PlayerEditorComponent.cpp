#include "PlayerEditorComponent.h"
#include <imgui.h>
#include <cmath> 
#include "Actor/Actor.h"
#include "Model.h"
#include "Graphics.h"
#include "Model/ModelRenderer.h"
#include "Gizmos.h"
#include <DirectXMath.h>
#include "Runner/RunnerComponent.h"
#include "Timeline/TimelineSequencerComponent.h"
#include "Collision/ColliderComponent.h"
#include "Storage/GEStorageCompilerComponent.h"
#include "Storage/GameplayAsset.h"
#include "JSONManager.h"
#include "System/Dialog.h"
#include <Input/InputActionComponent.h>
#include "Effect/EffectManager.h"
#include <filesystem> 
#include <algorithm>

#include <Windows.h>
#include <commdlg.h> 

namespace {
    inline DirectX::XMFLOAT3 TransformPoint(const DirectX::XMFLOAT3& p, const DirectX::XMFLOAT4X4& m)
    {
        using namespace DirectX;
        XMVECTOR v = XMVectorSet(p.x, p.y, p.z, 1.0f);
        XMMATRIX M = XMLoadFloat4x4(&m);
        XMVECTOR t = XMVector4Transform(v, M);
        DirectX::XMFLOAT3 out; XMStoreFloat3(&out, t); return out;
    }
    inline DirectX::XMFLOAT3 RotateByQuat(const DirectX::XMFLOAT3& v, const DirectX::XMFLOAT4& q)
    {
        using namespace DirectX;
        XMVECTOR V = XMLoadFloat3(&v);
        XMVECTOR Q = XMLoadFloat4(&q);
        XMVECTOR R = XMVector3Rotate(V, Q);
        DirectX::XMFLOAT3 out; XMStoreFloat3(&out, R); return out;
    }
    inline DirectX::XMFLOAT4 RgbaU32ToFloat4(unsigned rgba)
    {
        float a = ((rgba >> 24) & 0xFF) / 255.0f;
        float r = ((rgba >> 16) & 0xFF) / 255.0f;
        float g = ((rgba >> 8) & 0xFF) / 255.0f;
        float b = ((rgba) & 0xFF) / 255.0f;
        return DirectX::XMFLOAT4{ r,g,b,a };
    }

    bool SaveJsonDialog(char* outPath, int len, HWND owner)
    {
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner;
        ofn.lpstrFile = outPath;
        ofn.nMaxFile = len;
        ofn.lpstrFilter = "Gameplay Data (*.json)\0*.json\0All Files\0*.*\0\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrDefExt = "json";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        outPath[0] = '\0';
        return GetSaveFileNameA(&ofn) != FALSE;
    }
}

//---------------------------------------------
//---------------------------------------------
const char* PlayerEditorComponent::GetName() const
{
    return "PlayerEditor";
}

void PlayerEditorComponent::Start()
{
    LoadRecents();

    if (auto owner = GetActor())
    {
        auto runner = owner->GetComponent<RunnerComponent>();
        auto seq = owner->GetComponent<TimelineSequencerComponent>();

        if (seq && runner) { seq->SetRunner(runner.get()); }
    }
}

void PlayerEditorComponent::Update(float dt)
{
    if (auto owner = GetActor())
    {
        if (auto runner = owner->GetComponent<RunnerComponent>())
        {
            clipLength = runner->GetClipLength();

            ApplyScrubToModel();
        }
    }
}

void PlayerEditorComponent::Render()
{
    auto owner = GetActor();                   if (!owner) return;
    ::Model* model = owner->GetModelRaw();     if (!model) return;
}


void PlayerEditorComponent::OnGUI()
{
    DrawMainMenu();

    DrawAnimationsWindow();
    DrawHierarchyWindow();
    DrawEventsWindow();
}

void PlayerEditorComponent::DrawMainMenu()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open Model..."))
            {
                LoadModelFromDialog();
            }
            // Open Recent ...
            if (ImGui::BeginMenu("Open Recent"))
            {
                if (recentModelPaths.empty()) {
                    ImGui::MenuItem("(Empty)", nullptr, false, false);
                }
                else {
                    for (size_t i = 0; i < recentModelPaths.size(); ++i) {
                        const std::string& path = recentModelPaths[i];
                        if (ImGui::MenuItem(path.c_str())) {
                            std::shared_ptr<Actor> owner = GetActor();
                            if (owner) {
                                owner->LoadModel(path.c_str(), 0.1f);
                                AddRecentModel(path);
                                currentModelName = std::filesystem::path(path).stem().string();
                                if (auto m = owner->GetModelRaw()) {
                                    const auto& anims = m->GetAnimations();
                                    cachedTimelines.clear();
                                    ResizeCache((int)anims.size());

                                    if (auto compiler = owner->GetComponent<GEStorageCompilerComponent>()) {
                                        GameplayAsset data = compiler->LoadGameplayData(currentModelName);
                                        cachedTimelines = data.timelines;
                                        cachedCurves = data.curves;
                                        if (!cachedTimelines.empty()) ResizeCache((int)anims.size());
                                    }

                                    selectedAnimation = anims.empty() ? -1 : 0;
                                    clipLength = (selectedAnimation >= 0) ? anims[selectedAnimation].secondsLength : 3.0f;
                                    if (auto runner = owner->GetComponent<RunnerComponent>()) {
                                        runner->SetClipLength(clipLength);
                                        runner->SetTimeSeconds(0.0f);
                                        runner->Pause();
                                    }
                                    if (auto seq = owner->GetComponent<TimelineSequencerComponent>()) {
                                        if (selectedAnimation >= 0) seq->BindAnimation(selectedAnimation, clipLength);
                                        else seq->BindAnimation(-1, 0.0f);
                                    }
                                    LoadTimelineFromCache(selectedAnimation);
                                    ApplyScrubToModel();
                                }
                            }
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Clear Recent")) {
                        recentModelPaths.clear();
                        SaveRecents();
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        bool hasModel = !currentModelName.empty();

        // Save
        if (ImGui::MenuItem("Save"))
        {
            if (hasModel) SaveGameplayDataAs();
        }
        if (!hasModel && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("Please open a model first.");
        }

        // Load
        if (ImGui::MenuItem("Load"))
        {
            if (hasModel) LoadGameplayDataOpen();
        }
        if (!hasModel && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("Please open a model first.");
        }

        ImGui::EndMainMenuBar();
    }
}

void PlayerEditorComponent::SaveGameplayDataAs()
{
    std::shared_ptr<Actor> owner = GetActor();
    if (!owner) return;

    SaveTimelineToCache(selectedAnimation);

    char path[MAX_PATH] = {};
    std::string defaultName = currentModelName.empty() ? "GameplayData" : (currentModelName + "_Gameplay.json");
    strcpy_s(path, defaultName.c_str());

    HWND hwnd = GetActiveWindow();

    if (SaveJsonDialog(path, MAX_PATH, hwnd))
    {
        if (auto compiler = owner->GetComponent<GEStorageCompilerComponent>())
        {
            compiler->SaveGameplayDataToPath(path, cachedTimelines, cachedCurves);
        }
    }
}

void PlayerEditorComponent::LoadGameplayDataOpen()
{
    std::shared_ptr<Actor> owner = GetActor();
    if (!owner) return;

    char path[MAX_PATH] = {};
    const char* filter = "Gameplay Data (*.json)\0*.json\0All Files (*.*)\0*.*\0\0";

    HWND hwnd = GetActiveWindow();

    if (Dialog::OpenFileName(path, MAX_PATH, filter, "Load Gameplay Data", hwnd) == DialogResult::OK)
    {
        if (auto compiler = owner->GetComponent<GEStorageCompilerComponent>())
        {
            GameplayAsset data = compiler->LoadGameplayDataFromPath(path);
            cachedTimelines = data.timelines;
            cachedCurves = data.curves;

            LoadTimelineFromCache(selectedAnimation);
        }
    }
}



void PlayerEditorComponent::DrawEventsWindow() {}
void PlayerEditorComponent::DrawHierarchyWindow()
{
    if (!ImGui::Begin("Hierarchy")) { ImGui::End(); return; }
    std::shared_ptr<Actor> owner = GetActor();
    Model* model = owner ? owner->GetModelRaw() : nullptr;
    if (!model) { ImGui::TextUnformatted("(No model)"); ImGui::End(); return; }
    const auto& nodes = model->GetNodes();
    if (nodes.empty()) { ImGui::TextUnformatted("(No nodes)"); ImGui::End(); return; }
    DrawNodeRecursive(model, 0);
    ImGui::End();
}
void PlayerEditorComponent::DrawNodeRecursive(Model* model, int nodeIndex)
{
    const auto& nodes = model->GetNodes();
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) { return; }
    const auto& node = nodes[nodeIndex];
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (selectedNodeIndex == nodeIndex) { flags |= ImGuiTreeNodeFlags_Selected; }
    if (node.children.empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }
    bool open = ImGui::TreeNodeEx((void*)(intptr_t)nodeIndex, flags, "%s", node.name.c_str());
    if (ImGui::IsItemClicked()) { selectedNodeIndex = nodeIndex; }
    if (open) {
        for (const auto* child : node.children) {
            int idx = static_cast<int>(child - &nodes[0]);
            DrawNodeRecursive(model, idx);
        }
        ImGui::TreePop();
    }
}


void PlayerEditorComponent::DrawAnimationsWindow()
{
    if (!ImGui::Begin("Animations")) { ImGui::End(); return; }

    std::shared_ptr<Actor> owner = GetActor();
    Model* model = owner ? owner->GetModelRaw() : nullptr;
    if (!model) { ImGui::TextUnformatted("(No model)"); ImGui::End(); return; }

    const auto& anims = model->GetAnimations();
    if (anims.empty()) { ImGui::TextUnformatted("(No animations)"); ImGui::End(); return; }

    for (int i = 0; i < (int)anims.size(); ++i)
    {
        const auto& a = anims[i];
        if (searchText[0] != '\0')
        {
            if (a.name.find(searchText) == std::string::npos) { continue; }
        }

        const bool isSelected = (selectedAnimation == i);

        if (ImGui::Selectable(a.name.c_str(), isSelected))
        {

            if (selectedAnimation >= 0) {
                SaveTimelineToCache(selectedAnimation);
            }

            selectedAnimation = i;
            clipLength = a.secondsLength;

            if (auto owner2 = GetActor())
            {
                if (auto runner = owner2->GetComponent<RunnerComponent>())
                {
                    runner->SetClipLength(clipLength);
                    runner->SetTimeSeconds(0.0f);
                    runner->Play();
                }
                if (auto seq = owner2->GetComponent<TimelineSequencerComponent>())
                {
                    seq->BindAnimation(selectedAnimation, clipLength);
                }
            }

            LoadTimelineFromCache(selectedAnimation);

            ApplyScrubToModel();
        }
    }

    ImGui::End();
}


void PlayerEditorComponent::ApplyScrubToModel()
{
    std::shared_ptr<Actor> owner = GetActor();
    Model* model = owner ? owner->GetModelRaw() : nullptr;
    if (!model) { return; }
    if (selectedAnimation < 0) { return; }

    auto runner = owner->GetComponent<RunnerComponent>();
    if (!runner) { return; }

    const float tsec = runner->GetTimeSeconds();

    std::vector<Model::NodePose> poses;
    model->GetNodePoses(poses);
    model->ComputeAnimation(selectedAnimation, tsec, poses);

    auto& nodes = model->GetNodes();
    const int countNodes = static_cast<int>(nodes.size());
    const int countPoses = static_cast<int>(poses.size());
    const int count = (countNodes < countPoses) ? countNodes : countPoses;
    for (int i = 0; i < count; ++i)
    {
        nodes[i].position = poses[i].position;
        nodes[i].rotation = poses[i].rotation;
        nodes[i].scale = poses[i].scale;
    }

    const DirectX::XMFLOAT4X4& W = owner->GetTransform();
    model->UpdateTransform(W);
}


void PlayerEditorComponent::LoadModelFromDialog()
{
    char path[MAX_PATH] = {};
    const char* filter = "Model Files (*.fbx;*.obj;*.gltf;*.glb)\0*.fbx;*.obj;*.gltf;*.glb\0All Files (*.*)\0*.*\0\0";
    DialogResult r = Dialog::OpenFileName(path, sizeof(path), filter, nullptr, nullptr);
    if (r != DialogResult::OK) { return; }

    std::shared_ptr<Actor> owner = GetActor();
    if (!owner) { return; }

    owner->LoadModel(path, 0.1f);

    AddRecentModel(path);

    currentModelName = std::filesystem::path(path).stem().string();

    if (auto m = owner->GetModelRaw())
    {
        const auto& anims = m->GetAnimations();

        cachedTimelines.clear();
        cachedCurves.clear();

        ResizeCache((int)anims.size());

        if (auto compiler = owner->GetComponent<GEStorageCompilerComponent>()) {
            GameplayAsset data = compiler->LoadGameplayData(currentModelName);
            cachedTimelines = data.timelines;
            cachedCurves = data.curves;
            if (!cachedTimelines.empty()) {
                ResizeCache((int)anims.size());
            }
        }

        selectedAnimation = anims.empty() ? -1 : 0;
        clipLength = (selectedAnimation >= 0) ? anims[selectedAnimation].secondsLength : 3.0f;

        if (auto runner = owner->GetComponent<RunnerComponent>())
        {
            runner->SetClipLength(clipLength);
            runner->SetTimeSeconds(0.0f);
            runner->Pause();
        }
        if (auto seq = owner->GetComponent<TimelineSequencerComponent>())
        {
            if (selectedAnimation >= 0)
                seq->BindAnimation(selectedAnimation, clipLength);
            else
                seq->BindAnimation(-1, 0.0f);
        }

        LoadTimelineFromCache(selectedAnimation);
    }

    /* if (autoScaleOnModelLoad) { AutoScaleActorForPreview(); }*/
}


void PlayerEditorComponent::AddRecentModel(const std::string& path)
{
    for (size_t i = 0; i < recentModelPaths.size(); ++i)
    {
        if (recentModelPaths[i] == path)
        {
            recentModelPaths.erase(recentModelPaths.begin() + static_cast<long long>(i));
            break;
        }
    }
    recentModelPaths.insert(recentModelPaths.begin(), path);
    while (recentModelPaths.size() > 10) { recentModelPaths.pop_back(); }
    SaveRecents();
}


void PlayerEditorComponent::LoadRecents()
{
    recentStore.Load();
    recentModelPaths = recentStore.Get<std::vector<std::string>>("recentModels", {});
}

void PlayerEditorComponent::SaveRecents()
{
    recentStore.Set("recentModels", recentModelPaths);
    recentStore.Save();
}


float PlayerEditorComponent::ComputeApproxActorRadiusByNodes(const Model* model) const
{
    if (!model) { return 1.0f; }

    const auto& nodes = model->GetNodes();
    if (nodes.size() < 2) { return 1.0f; }

    DirectX::XMFLOAT3 minv = { 1e9f,  1e9f,  1e9f };
    DirectX::XMFLOAT3 maxv = { -1e9f, -1e9f, -1e9f };

    for (const auto& n : nodes)
    {
        const DirectX::XMFLOAT3& p = n.position;
        if (p.x < minv.x) minv.x = p.x; if (p.x > maxv.x) maxv.x = p.x;
        if (p.y < minv.y) minv.y = p.y; if (p.y > maxv.y) maxv.y = p.y;
        if (p.z < minv.z) minv.z = p.z; if (p.z > maxv.z) maxv.z = p.z;
    }

    const float dx = maxv.x - minv.x;
    const float dy = maxv.y - minv.y;
    const float dz = maxv.z - minv.z;
    const float len = sqrtf(dx * dx + dy * dy + dz * dz);
    const float radius = 0.5f * (len <= 0.0001f ? 2.0f : len);

    return (radius <= 0.0001f) ? 1.0f : radius;
}


void PlayerEditorComponent::AutoScaleActorForPreview()
{
    std::shared_ptr<Actor> owner = GetActor();
    Model* model = owner ? owner->GetModelRaw() : nullptr;
    if (!owner || !model) { return; }

    const float currentRadius = ComputeApproxActorRadiusByNodes(model);
    float s = (currentRadius > 0.0001f) ? (previewDesiredRadius / currentRadius) : 1.0f;

    if (s < 0.01f)  s = 0.01f;
    if (s > 100.0f) s = 100.0f;

    owner->SetScale(DirectX::XMFLOAT3{ s, s, s });
}

// ========================================================================
// ========================================================================

void PlayerEditorComponent::ResizeCache(int size)
{
    if (size < 0) size = 0;
    if ((int)cachedTimelines.size() < size) {
        cachedTimelines.resize(size);
        cachedCurves.resize(size);
    }
}

void PlayerEditorComponent::SaveTimelineToCache(int animIndex)
{
    if (animIndex < 0) return;
    ResizeCache(animIndex + 1);

    if (auto owner = GetActor()) {
        if (auto seq = owner->GetComponent<TimelineSequencerComponent>()) {
            cachedTimelines[animIndex] = seq->GetItems();
            cachedCurves[animIndex] = seq->GetCurveSettings();
        }
    }
}

void PlayerEditorComponent::LoadTimelineFromCache(int animIndex)
{
    if (auto owner = GetActor()) {
        if (auto seq = owner->GetComponent<TimelineSequencerComponent>()) {
            if (animIndex >= 0 && animIndex < (int)cachedTimelines.size()) {
                seq->GetItemsMutable() = cachedTimelines[animIndex];
                if (animIndex < (int)cachedCurves.size()) {
                    seq->SetCurveSettings(cachedCurves[animIndex]);
                }
                else {
                    seq->SetCurveSettings({});
                }
            }
            else {
                seq->GetItemsMutable().clear();
                seq->SetCurveSettings({});
            }
        }
    }
}

