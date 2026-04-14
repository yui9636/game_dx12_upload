# Player Editor Authoring UI Practicalization Spec

Date: 2026-04-11  
Workspace: `game_dx12_upload`

---

## 1. Purpose

This document defines the next-phase overhaul of `PlayerEditor` authoring UI after the runtime integration path was unified.

The goal is not visual polish. The goal is to stop shipping fake editing surfaces.

Current `PlayerEditor` authoring is still not a practical tool:

- `StateMachine` can technically create nodes and transitions, but the authoring flow is not obvious, not guided, and not close to immediate gameplay testing.
- `Timeline` can technically add tracks and items, but track/item creation is too generic, payload editing is fragmented, and animation-event authoring does not exist as a usable workflow.
- `Input` can now save/load, but the state machine authoring flow still does not make input-driven gameplay obvious.

This spec upgrades `PlayerEditor` into a usable authoring tool for:

- player input action definition
- state machine authoring
- timeline track/item authoring
- animation-event authoring per animation
- immediate preview and gameplay validation

This is explicitly a practicalization spec. It is not a generic editor modernization memo.

---

## 2. Current Problems

### 2.1 `PlayerEditorPanel.cpp` is still a god-file

`Source/PlayerEditor/PlayerEditorPanel.cpp` owns:

- dock layout
- toolbar
- viewport
- skeleton tree
- state machine graph
- timeline track list and grid
- properties panel
- animator panel
- input panel
- asset open/save
- preview wiring

That was tolerable while the tool was fake. It becomes a maintenance trap once authoring grows. This spec does not require full decomposition now, but all new authoring UI must be added with explicit internal sub-responsibility boundaries.

Minimum split boundary inside `PlayerEditorPanel.cpp`:

- state machine graph interactions
- state machine inspector interactions
- timeline creation/selection interactions
- timeline payload inspector interactions
- animation event authoring interactions

### 2.2 `StateMachine` authoring is not practical

Current asset model in `Source/PlayerEditor/StateMachineAsset.h` is too thin for practical authoring:

- `StateNode` only stores `animationIndex`, `timelineAssetPath`, `loopAnimation`, `animSpeed`, `canInterrupt`, and generic `properties`
- no authoring-level link to action/input intent
- no state preview command
- no readable summary of what makes a transition fire
- no guided authoring for locomotion/action/dodge style states

Current UI in `Source/PlayerEditor/PlayerEditorPanel.cpp` is also weak:

- graph node creation is bare minimum
- transition editing is inspector-only and not obvious
- parameter creation exists, but the overall state machine still feels like raw structs
- input condition setup depends on users understanding action names and transition condition semantics upfront

This is not a usable gameplay state editor. It is a debug surface over a raw asset.

### 2.3 `Timeline` authoring is generic to the point of uselessness

Current timeline behavior in `Source/PlayerEditor/PlayerEditorPanel.cpp`:

- `+ Track` exposes every track type
- `Add Item Here` creates generic `TimelineItem` ranges with almost no guided defaults
- item interaction is mostly move-only
- there is no strong track-type-specific creation flow
- event authoring is effectively absent as a practical workflow
- selection model is weak for editing multiple timeline items in the same animation

Current `TimelineAsset` in `Source/PlayerEditor/TimelineAsset.h` is serviceable as storage, but the UI on top of it is bad:

- track types exist, but creation UX does not match the payload shape
- `items` and `keyframes` are both supported structurally, but authoring flow does not explain when to use either
- animation events are stored only as generic `eventName/eventData`, not surfaced as first-class animation-event workflow

### 2.4 Animation-specific event authoring does not exist

The user requirement is clear:

- events must be saved per animation
- timeline authoring must be grounded in animation ranges

Current state:

- `TimelineAsset` has `ownerModelPath` and `animationIndex`
- but there is no dedicated authoring mode that says:
  - "You are editing events for animation X"
  - "These event tracks belong to animation X"
  - "The event timeline duration is derived from animation X"

Without this, timeline data is still too easy to author against the wrong clip.

### 2.5 Collider / effect placement is not practical enough

Current payload inspector supports:

- `nodeIndex`
- `offsetLocal`
- radius / scale / rotation

But the workflow is still weak:

- node selection is manual integer editing in too many places
- no clear "pick selected bone/socket" path everywhere
- collider size editing is present but not surfaced as a core action flow
- VFX placement is editable but not intuitive

Functionally the data exists. Usability-wise it is still close to garbage.

---

## 3. Design Principles

### 3.1 State machine authoring must be Unity-like in readability, not in cargo-cult visuals

Do not imitate Unity mechanically.

What must be borrowed is:

- immediate readability of current state
- obvious transition direction
- obvious transition conditions
- obvious default state
- obvious preview/testing entry point
- obvious parameter/input relationship

What must not be borrowed blindly:

- overcomplicated animator sub-state machine hierarchy
- generalized blend tree system in v1
- giant inspector dependency on hidden controller semantics

For this engine, v1 practical state machine means:

- state graph
- parameters
- transitions
- animation assignment
- timeline assignment
- input-driven transitions
- one-click preview of state behavior

Nothing more is required for v1.

### 3.2 Input authoring must be done here, not mentally reconstructed elsewhere

The user must be able to:

1. define/edit actions in `Input`
2. reference those actions directly in `StateMachine`
3. preview the resulting state behavior immediately

If the user has to remember action names manually and mentally map them while authoring transitions, the tool failed.

### 3.3 Timeline authoring must be animation-scoped

Timeline editing must be anchored to a concrete animation.

That means:

- one selected animation defines the clip duration baseline
- event authoring is saved against that animation context
- track/item authoring is done relative to the animation frame range
- authoring UI always shows which animation is currently being edited

### 3.4 Track creation must be type-driven, not generic

Adding a `Hitbox` track should not feel the same as adding an `Event` track.

Track creation must apply:

- correct default naming
- correct default payload
- correct default item creation behavior
- correct inspector

The current generic "add empty track, then hand edit raw fields" approach is too primitive.

---

## 4. Scope

Files primarily affected:

- `Source/PlayerEditor/PlayerEditorPanel.cpp`
- `Source/PlayerEditor/PlayerEditorPanel.h`
- `Source/PlayerEditor/TimelineAsset.h`
- `Source/PlayerEditor/TimelineAssetSerializer.cpp`
- `Source/PlayerEditor/StateMachineAsset.h`
- `Source/PlayerEditor/StateMachineAssetSerializer.cpp`
- `Source/PlayerEditor/InputMappingTab.cpp`
- `Source/PlayerEditor/InputMappingTab.h`

Secondary affected files:

- `Source/Gameplay/StateMachineSystem.cpp`
- `Source/Gameplay/TimelineAssetRuntimeBuilder.cpp`
- `Source/Input/InputActionMapAsset.cpp`

Out of scope for this document:

- blend trees
- nested state machines
- cinematic camera authoring parity
- undo/redo redesign
- full generic node-graph framework extraction

---

## 5. State Machine Authoring Spec

### 5.1 Core Authoring Goal

`StateMachine` authoring must become the primary gameplay behavior editor.

The user must be able to:

1. create a state
2. assign animation
3. assign optional timeline
4. connect transitions
5. choose input/parameter conditions from readable UI
6. hit preview
7. see state behavior immediately

If any of those steps still requires raw path typing, numeric animation guessing, or manual name recall, v1 failed.

### 5.2 State List + Graph Dual View

Current graph-only editing is weak. Add a dual representation:

- left-side `State List`
- center `Graph`

`State List` must show:

- state name
- state type
- assigned animation name
- assigned timeline asset short name
- outgoing transition count
- default-state marker

`Graph` remains for spatial authoring and visual transition overview.

This is necessary because graph-only editing becomes unreadable fast, and current graph interactions are too light.

### 5.3 State Creation Templates

`Add State` must no longer create an anonymous raw state only.

Templates required:

- `Locomotion`
- `Action`
- `Dodge`
- `Jump`
- `Damage`
- `Dead`
- `Custom`

Template behavior:

- creates state with type-specific default name
- sets sane loop default
- sets interrupt default
- sets recommended transition behavior baseline

Example:

- `Locomotion`: loop on, interrupt on
- `Action`: loop off, interrupt off by default
- `Dodge`: loop off, interrupt off

This is not runtime logic. This is authoring UX.

### 5.4 Input-Driven Transition Authoring

Current transition condition UI is still too raw.

Required behavior:

- `ConditionType::Input` must always use action selection UI first when an action map exists
- action list must show action names from current input map
- transition row must render readable text summary:
  - `Attack pressed`
  - `MoveX > 0.1`
  - `AnimEnd == true`
  - `Timer >= 0.2`

Required UI additions:

- `Add Transition` quick action on selected state
- transition row summary chips
- one-click add common conditions:
  - `On Press`
  - `On Release`
  - `On Hold`
  - `Anim End`
  - `Exit Time`

If `Input` has no loaded action map, transition UI must explicitly say so.

### 5.5 Input and State Machine Co-Authoring

`StateMachine` panel must include a compact read-only current action map summary:

- current input map asset name
- actions count
- button to jump to `Input` panel

Transition editor must never assume users remember action names manually.

### 5.6 State Preview

Selected state must have:

- `Preview State`
- `Preview From Start`
- `Preview Loop`

Behavior:

- preview forces selected state animation + selected state timeline
- preview does not require full gameplay transition entry
- preview updates runtime timeline data through the shared runtime path

This is editor-only control over runtime-compatible data.

### 5.7 Parameter Authoring

Parameter UI must remain simple:

- list name
- type
- default value

But state machine graph and transition inspector must surface parameters clearly:

- transition conditions referencing parameter names must use dropdown when possible
- parameter values must be preview-editable during preview session

That preview edit path is critical for testing state flow without entering full game runtime.

---

## 6. Timeline Authoring Spec

### 6.1 Timeline Authoring Mode is Animation-Scoped

Timeline panel must always show:

- current model
- current animation name
- frame count
- seconds

If a timeline is not bound to an animation, the UI must say so explicitly.

Editing flow:

1. choose animation
2. create/open timeline for that animation
3. author tracks/items/events for that animation

`TimelineAsset.ownerModelPath` and `TimelineAsset.animationIndex` become first-class visible authoring context, not hidden storage.

### 6.2 Track Types for v1

Practical v1 track types:

- `Hitbox`
- `VFX`
- `Audio`
- `CameraShake`
- `Event`

De-emphasize in UI for now:

- `Animation`
- `Camera`
- `Custom`

These may still exist in data, but v1 UI must not pretend they are equally production-ready.

### 6.3 Track Creation UX

`+ Track` must open a typed creation popup with:

- track type
- default generated name
- whether to auto-create first item at current playhead

Default generated names:

- `Hitbox 01`
- `VFX 01`
- `Audio 01`
- `Shake 01`
- `Event 01`

If auto-create is enabled, track-specific first item defaults apply.

### 6.4 Item Creation UX

Current `Add Item Here` is too generic. Replace with type-specific insertion:

- `Add Range` for `Hitbox / VFX / Audio / CameraShake`
- `Add Event` for `Event`

Range defaults:

- start = current playhead
- end = current playhead + default duration
- duration defaults vary by type

Event defaults:

- frame = current playhead
- default event name empty but highlighted

### 6.5 Selection Model

The user requirement is explicit:

- select range
- select track

Required selection model:

- selected track highlight
- selected item highlight
- selected range drag handles
- selected track header actions

Track selection must drive inspector context.
Item selection must drive payload inspector context.

### 6.6 Range Editing

Range items must support:

- move
- left-resize
- right-resize
- duplicate
- delete

Current move-only behavior is not acceptable.

### 6.7 Collider / VFX Practical Editing

For `Hitbox` and `VFX`, the inspector must support:

- `Use Selected Bone`
- `Use Selected Socket`
- node name display
- node index fallback display
- offset editing
- size/radius editing
- quick reset buttons

`Hitbox` minimum practical payload:

- target node or socket
- local offset
- radius
- color

`VFX` minimum practical payload:

- asset id/path
- target node or socket
- local offset position
- local offset rotation
- local offset scale

This is already partially present in raw form. The missing part is guided selection and readability.

### 6.8 Event Authoring per Animation

Animation events must be authorable as timeline events.

v1 storage rule:

- event authoring remains in `TimelineAsset`
- one timeline asset is authored for one model + one animation
- therefore event data is effectively stored per animation context

Required event track behavior:

- event entries are point events, not fake ranges
- event item fields:
  - `eventName`
  - `eventData`
- event list must be visible in a compact side list for the current animation

The point is to make "animation-specific event saving" practical without inventing a second event asset system right now.

### 6.9 Playhead and Preview

Timeline playhead must remain the main editing cursor.

Required:

- current frame
- current time
- animation duration
- preview playback state

When a timeline item is selected, preview scrubbing must reflect the same runtime data used by game systems.

---

## 7. Asset Model Changes

### 7.1 `StateMachineAsset.h`

Additions required:

- optional editor-only display color per state type is not required
- no large asset model rewrite needed for v1
- keep `StateNode` and `StateTransition` mostly stable

But add editor-facing helper capability:

- compact helpers for readable summary generation
- helper for default authoring name creation

Do not bloat asset format just to compensate for bad UI.

### 7.2 `TimelineAsset.h`

Current structure is almost usable, but v1 authoring wants clearer distinction:

- range tracks:
  - `Hitbox`
  - `VFX`
  - `Audio`
  - `CameraShake`
- point tracks:
  - `Event`

Required additions:

- event track must use either `TimelineKeyframe`-based event entries or dedicated item behavior
- current generic `eventName/eventData` on `TimelineItem` is acceptable for v1 only if event track UI treats them as point events

Do not introduce a second parallel event storage path for v1.

### 7.3 Socket / Node Reference Authoring

Current payloads reference `nodeIndex`.

For authoring UX, add editor-level support for:

- selected bone name
- selected socket name
- resolved node index preview

Runtime can still consume numeric node index for now, but authoring UI must stop forcing users to think in integers.

---

## 8. UI Layout Changes

### 8.1 State Machine Panel

Rebuild into:

- top mini-toolbar
  - `Add State`
  - `Add Transition`
  - `Preview State`
  - `Set Default`
- left `State List`
- center `Graph`
- right `Inspector`

### 8.2 Timeline Panel

Rebuild into:

- top playback/animation context bar
- left track list
- center timeline grid
- optional right-side compact item/event inspector

### 8.3 Input Panel

Keep existing base, but add:

- current action map summary banner
- quick action creation
- better tie-in with state machine action references

---

## 9. Runtime Connection Rules

### 9.1 Authoring UI must use existing runtime path

Do not regress to editor-only fake data flow.

State preview and timeline preview must keep using:

- shared `TimelineAssetRuntimeBuilder`
- shared `TimelineComponent`
- shared `TimelineItemBuffer`

Editor-only additions are allowed only for:

- selection
- overlays
- preview camera
- preview forcing of state/time

### 9.2 Event runtime path

If event tracks become preview-visible, runtime handling must remain compatible with the same timeline asset.

Do not create a separate editor-only event interpretation.

---

## 10. Implementation Phases

### Phase 1: State Machine Practicalization

- add state list
- add state templates
- add quick transition creation
- add readable transition summaries
- force action dropdown usage when input map exists
- add selected-state preview actions

### Phase 2: Timeline Practicalization

- typed track creation popup
- typed item creation
- range resize interactions
- selected track / selected item behavior cleanup
- hitbox/vfx practical inspector improvements

### Phase 3: Animation Event Workflow

- animation-scoped event authoring UI
- event track point editing
- compact event list for current animation
- save/load through existing timeline serializer path

### Phase 4: Preview Usability

- preview overlays for selected collider/effect anchor
- better selected node/socket authoring feedback
- preview state/time controls that remain runtime-compatible

---

## 11. Acceptance Criteria

The spec is complete only when all items below are true.

### 11.1 State Machine

- a user can create a state machine without typing raw action names manually
- a user can see default state clearly
- a user can add a transition in under 3 interactions from a selected state
- a user can preview a selected state directly from the state machine panel
- a user can understand why a transition fires by reading its summary row

### 11.2 Timeline

- a user can add a `Hitbox`, `VFX`, `Audio`, `CameraShake`, or `Event` track with typed defaults
- a user can add a timeline item appropriate to the selected track type
- a user can move and resize range items
- a user can select a track and a specific item cleanly
- a user can assign node/socket, offset, and size/radius for collider and VFX payloads without raw index-only workflow

### 11.3 Animation Event Workflow

- a user can author event entries against a selected animation
- event entries are saved with the timeline asset bound to that animation context
- reopening the timeline for the same animation restores those event entries

### 11.4 Runtime Trustworthiness

- preview still uses runtime-compatible timeline data
- a timeline authored in the editor still reaches `GameLayer` through the existing runtime path
- no new editor-only fake execution path is introduced

---

## 12. First Implementation Tasks

1. Refactor `PlayerEditorPanel.cpp` internal state machine section into explicit helper blocks for list, graph, and inspector.
2. Add `State List` UI with default marker, animation label, timeline label, and transition count.
3. Replace raw state creation with typed state templates.
4. Upgrade transition condition rows to readable summary-based authoring.
5. Add `Preview State` and `Preview From Start` actions for selected state.
6. Replace generic timeline track creation popup with typed track creation.
7. Implement range resize handles in timeline grid.
8. Split event track authoring from generic range-item flow.
9. Add `Use Selected Bone` / `Use Selected Socket` workflow across hitbox and VFX inspectors.
10. Add animation-context header and event list to the timeline panel.

---

## 13. Final Judgment

Current `StateMachine` and `Timeline` UI are not merely incomplete. They are misleading.

They advertise authoring capability but mostly expose raw structs with weak interaction design. That is why users can touch the tool yet still fail to produce practical gameplay data quickly.

This spec intentionally does not ask for "more features". It asks for:

- less fake flexibility
- more guided authoring
- tighter animation scope
- tighter input-to-state authoring
- tighter track-type-specific editing

If implemented correctly, this turns `PlayerEditor` from a flashy prototype into a tool that can actually produce player behavior data without constant code-side rescue.
