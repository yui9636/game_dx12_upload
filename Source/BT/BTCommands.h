#pragma once
#include "System/UndoSystem.h"
#include "BTGraph.h"
#include <imgui_node_editor.h>

namespace ed = ax::NodeEditor;

class CmdBTAddLink : public ICommand {
public:
    CmdBTAddLink(BTGraph* graph, const BTNodeLink& link) : m_Graph(graph), m_Link(link) {}
    void Execute() override { m_Graph->links.push_back(m_Link); }
    void Undo() override {
        auto id = m_Link.id;
        m_Graph->links.erase(std::remove_if(m_Graph->links.begin(), m_Graph->links.end(),
            [id](auto& l) { return l.id == id; }), m_Graph->links.end());
    }
    const char* GetName() const override { return "Add BT Link"; }
private:
    BTGraph* m_Graph; BTNodeLink m_Link;
};

class CmdBTMoveNode : public ICommand {
public:
    CmdBTMoveNode(unsigned int nodeId, ImVec2 oldPos, ImVec2 newPos)
        : m_NodeId(nodeId), m_OldPos(oldPos), m_NewPos(newPos) {
    }
    void Execute() override { ed::SetNodePosition(m_NodeId, m_NewPos); }
    void Undo() override { ed::SetNodePosition(m_NodeId, m_OldPos); }
    const char* GetName() const override { return "Move BT Node"; }
private:
    unsigned int m_NodeId; ImVec2 m_OldPos, m_NewPos;
};

class CmdBTChangeWeight : public ICommand {
public:
    CmdBTChangeWeight(BTGraph* graph, unsigned int linkId, float oldW, float newW)
        : m_Graph(graph), m_LinkId(linkId), m_OldW(oldW), m_NewW(newW) {
    }
    void Execute() override { Set(m_NewW); }
    void Undo() override { Set(m_OldW); }
    const char* GetName() const override { return "Change Weight"; }
private:
    void Set(float w) {
        for (auto& l : m_Graph->links) if (l.id == m_LinkId) { l.weight = w; break; }
    }
    BTGraph* m_Graph; unsigned int m_LinkId; float m_OldW, m_NewW;
};

class CmdBTDeleteLink : public ICommand {
public:
    CmdBTDeleteLink(BTGraph* graph, const BTNodeLink& link) : m_Graph(graph), m_Link(link) {}
    void Execute() override {
        unsigned int id = m_Link.id;
        m_Graph->links.erase(std::remove_if(m_Graph->links.begin(), m_Graph->links.end(),
            [id](auto& l) { return l.id == id; }), m_Graph->links.end());
    }
    void Undo() override { m_Graph->links.push_back(m_Link); }
    const char* GetName() const override { return "Delete BT Link"; }
private:
    BTGraph* m_Graph; BTNodeLink m_Link;
};

class CmdBTDeleteNode : public ICommand {
public:
    CmdBTDeleteNode(BTGraph* graph, const BTNodeEditorData& node, const std::vector<BTNodeLink>& attachedLinks)
        : m_Graph(graph), m_Node(node), m_Links(attachedLinks) {
    }

    void Execute() override {
        unsigned int nid = m_Node.id;
        m_Graph->nodes.erase(std::remove_if(m_Graph->nodes.begin(), m_Graph->nodes.end(),
            [nid](auto& n) { return n.id == nid; }), m_Graph->nodes.end());
        for (const auto& al : m_Links) {
            unsigned int lid = al.id;
            m_Graph->links.erase(std::remove_if(m_Graph->links.begin(), m_Graph->links.end(),
                [lid](auto& l) { return l.id == lid; }), m_Graph->links.end());
        }
    }
    void Undo() override {
        m_Graph->nodes.push_back(m_Node);
        for (const auto& l : m_Links) m_Graph->links.push_back(l);
        ax::NodeEditor::SetNodePosition(m_Node.id, m_Node.pos);
    }
    const char* GetName() const override { return "Delete BT Node"; }
private:
    BTGraph* m_Graph; BTNodeEditorData m_Node; std::vector<BTNodeLink> m_Links;
};





