#pragma once

#include <string>
#include "Entity/Entity.h"

class PlayerEditorPanel;

class PlayerEditorSession
{
public:
    static void Suspend(PlayerEditorPanel& panel);

    static void DestroyOwnedPreviewEntity(PlayerEditorPanel& panel);
    static void EnsureOwnedPreviewEntity(PlayerEditorPanel& panel);
    static void SetPreviewEntity(PlayerEditorPanel& panel, EntityID entity);
    static void SyncExternalSelection(PlayerEditorPanel& panel, EntityID entity, const std::string& modelPath);

    static bool OpenModelFromPath(PlayerEditorPanel& panel, const std::string& path);
    static bool OpenTimelineFromPath(PlayerEditorPanel& panel, const std::string& path);
    static bool OpenStateMachineFromPath(PlayerEditorPanel& panel, const std::string& path);
    static bool OpenInputMapFromPath(PlayerEditorPanel& panel, const std::string& path);

    static bool SaveTimelineDocument(PlayerEditorPanel& panel, bool saveAs);
    static bool SaveStateMachineDocument(PlayerEditorPanel& panel, bool saveAs);
    static bool SaveInputMapDocument(PlayerEditorPanel& panel, bool saveAs);
    static bool SaveAllDocuments(PlayerEditorPanel& panel, bool saveAs);
    static bool SavePrefabDocument(PlayerEditorPanel& panel, bool saveAs);
    static void RevertAllDocuments(PlayerEditorPanel& panel);

    static void ApplyEditorBindingsToPreviewEntity(PlayerEditorPanel& panel);
    static void RebuildPreviewTimelineRuntimeData(PlayerEditorPanel& panel);
    static void SyncPreviewTimelinePlayback(PlayerEditorPanel& panel);
    static void ImportFromSelectedEntity(PlayerEditorPanel& panel);
    static void ImportSocketsFromPreviewEntity(PlayerEditorPanel& panel);
    static void ExportSocketsToPreviewEntity(PlayerEditorPanel& panel);
};
