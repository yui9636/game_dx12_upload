# AI Operation Session Specification

Phase 1 adds a session layer around the existing automation API. The goal is to make AI work auditable and reversible enough for safe iteration.

## Commands

### ai_session.begin

Starts an AI authoring session and creates:

- `Saved/AI/sessions/<session-id>/session.json`
- `Saved/AI/sessions/<session-id>/events.jsonl`
- `Saved/AI/sessions/<session-id>/screenshots/`

```json
{
  "version": 1,
  "id": "cmd-session-begin",
  "command": "ai_session.begin",
  "params": {
    "name": "Create duel arena",
    "goal": "Build and review a 1v1 action-game test arena.",
    "captureTargets": ["scene_view", "game_view"],
    "autoCaptureAfterCommand": false
  }
}
```

`force:true` replaces an unfinished active session.

### ai_session.status

Returns the active session id, log paths, command count, current engine state, ECS revision, and Undo counts.

### ai_session.rollback

Rolls back ECS Undo entries created after the session began.

```json
{
  "version": 1,
  "id": "cmd-session-rollback",
  "command": "ai_session.rollback",
  "params": {
    "undoSteps": 3
  }
}
```

If `undoSteps` is omitted, all Undo entries after the session start are undone. By default, session rollback also restores backed-up editable files and deletes newly created editable files under the configured backup roots.

File backup defaults:

- `backupFiles`: `true`
- `backupRoots`: `["Data"]`
- `backupExtensions`: `.scene`, `.prefab`, `.material`, `.mat`, `.terrain`, `.effectgraph`, `.json`, `.inputmap`, `.inputprofile`, `.gameflow`

Use `restoreFiles:false` on rollback when you only want ECS Undo rollback.

### ai_session.end

Finalizes `session.json` and marks the session inactive.

```json
{
  "version": 1,
  "id": "cmd-session-end",
  "command": "ai_session.end",
  "params": {
    "success": true,
    "notes": "Visual review passed."
  }
}
```

## Event Log

While a session is active, every non-session automation command records one JSON line with:

- command payload
- response payload
- before/after engine state
- before/after ECS revision
- before/after Undo count
- screenshots when `autoCaptureAfterCommand` is enabled, or when a command fails

This makes the AI loop replayable enough for human review and provides a safe entry point for rollback.

## Autosave Recovery Automation

The existing Autosave Recovery popup is also exposed to AI. This lets agents handle the same recovery candidate a human sees in the editor.

### editor.recovery.get_state

```json
{
  "version": 1,
  "id": "cmd-recovery-state",
  "command": "editor.recovery.get_state",
  "params": {
    "refresh": true
  }
}
```

Returns `hasCandidate`, `autosavePath`, and `scenePath`.

### editor.recovery.restore

Loads the pending autosave and keeps the original scene path as the save target.

### editor.recovery.dismiss

Clears the pending recovery candidate without loading it.
