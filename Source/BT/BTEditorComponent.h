#pragma once
#include "Component/Component.h"
#include "BTGraph.h"
#include <imgui_node_editor.h>
#include "BehaviorTree.h"

namespace ed = ax::NodeEditor;


struct BTAgentInfo {
    std::string name;
    std::map<unsigned int, BTNodeReport> reports;
    float lastUpdateTime;
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

    static std::map<void*, BTAgentInfo> s_AgentRegistry;
    static void* s_SelectedAgentPtr;
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
