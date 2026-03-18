#pragma once
#include "Component/Component.h"
#include "BTGraph.h"
#include <imgui_node_editor.h>
#include "BehaviorTree.h"

namespace ed = ax::NodeEditor;


struct BTAgentInfo {
    std::string name;                                // 個体名（"Mutant Boss" など）
    std::map<unsigned int, BTNodeReport> reports;    // その個体のノード実行状態
    float lastUpdateTime;                            // 最終更新時間（死んだやつをリストから消す用）
};


class BTEditorComponent : public Component
{
public:
    const char* GetName() const override { return "BTEditor"; }

    void Start() override;
    void OnGUI() override;
    void Update(float dt) override;

    void SaveGraph(const std::string& path);
    void LoadGraph(const std::string& path);

    static void PushLiveDebugData(const std::map<unsigned int, BTNodeReport>& data);

    static std::map<void*, BTAgentInfo> s_AgentRegistry; // AI個体ごとのデータ台帳
    static void* s_SelectedAgentPtr;                     // 現在エディタでロックしている個体
private:
    void DrawNodes();
    void DrawLinks();
    void DrawTraceTimeline();
    void DrawBlackboardWatcher();
    void HandleInteraction();
    void ShowSearchablePalette();

    void DrawDynamicInspector();  
    void ApplyNiagaraTheme();   

private:
    ed::EditorContext* m_Context = nullptr;
    BTGraph m_Graph;
    char m_searchFilter[64] = ""; 

    static std::map<unsigned int, BTNodeReport> s_LiveDebugData;
};