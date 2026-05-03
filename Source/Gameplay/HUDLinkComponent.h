#pragma once

// Bridges a HealthComponent to UI surfaces. HUDBindingSystem reads this every frame.
// Plain booleans so the routing is trivially editable from Inspector.
struct HUDLinkComponent {
    bool  asPlayerHUD   = false; // bottom-screen player bar
    bool  asBossHUD     = false; // top-screen boss bar
    bool  asWorldFloat  = true;  // floating 3D HP gauge above the head
    float worldOffsetY  = 2.0f;
};
