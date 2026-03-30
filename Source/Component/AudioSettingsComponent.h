#pragma once

struct AudioSettingsComponent
{
    float masterVolume = 1.0f;
    float bgmVolume = 1.0f;
    float sfxVolume = 1.0f;
    float uiVolume = 1.0f;
    bool muteAll = false;
    bool debugDraw = false;
};
