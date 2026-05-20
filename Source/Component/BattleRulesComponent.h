// BattleRulesComponent: configurable win/lose rules for a 1v1 action battle.
// Place this on a single setup entity; BattleFlowSystem reads it each frame to
// drive the encounter -> combat -> victory/defeat/draw flow.
#pragma once

struct BattleRulesComponent {
    // --- Encounter ---
    float encounterRadius = 18.0f;        // player entering this radius starts the battle
    float introDuration   = 1.5f;         // "FIGHT" intro length before combat begins
    bool  autoStartOnPlayerEnter = true;  // start automatically when player enters the arena

    // --- Time limit ---
    bool  enableTimeLimit  = false;       // when true, the combat phase is timed
    float timeLimitSeconds = 99.0f;       // combat duration before time runs out

    // --- Time-up resolution ---
    // 0 = higher HP ratio wins (draw if equal)
    // 1 = player loses (defeat)
    // 2 = draw
    int   timeUpResult = 0;

    // Win = enemy HP reaches 0. Lose = player HP reaches 0. (Always active.)
};
