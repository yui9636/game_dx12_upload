#pragma once

#include <filesystem>

#include "BehaviorTreeAsset.h"
#include "EnemyConfigAsset.h"

class Registry;

// Editor panel for authoring BehaviorTreeAsset (.bt) files.
// Also has a small EnemyConfig section (Phase 5) so 1v1 integration is achievable
// without a full PlayerEditor refactor.
class BehaviorTreeEditorPanel
{
public:
    BehaviorTreeEditorPanel();

    void Draw(Registry* registry, bool* p_open = nullptr, bool* outFocused = nullptr);

private:
    void DrawToolbar();
    void DrawTreeView();
    void DrawNodeInspector();
    void DrawValidateResult();
    void DrawEnemyConfigSection();
    void DrawBlackboardLivePeek(Registry* registry);

    void DoNewTemplate(int which); // 0=Aggressive 1=Defensive 2=Patrol
    void DoLoad(const std::filesystem::path& p);
    void DoSave();
    void DoSaveAs(const std::filesystem::path& p);
    void DoValidate();

    void DoEnemyConfigLoad(const std::filesystem::path& p);
    void DoEnemyConfigSave();

    BehaviorTreeAsset    m_btAsset;
    EnemyConfigAsset     m_enemyConfig;

    std::filesystem::path m_btPath     = "Data/AI/BehaviorTrees/AggressiveKnight.bt";
    std::filesystem::path m_enemyPath  = "Data/AI/Enemies/AggressiveKnight.enemy";

    int      m_selectedNodeIndex = -1;
    BTValidateResult m_lastValidate;
    bool     m_validatedOnce = false;

    char m_btLoadPathBuf[512]   = {};
    char m_btSaveAsPathBuf[512] = {};
    char m_enemyLoadPathBuf[512] = {};
};
