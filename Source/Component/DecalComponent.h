// DecalComponent: projects a texture onto G-buffer surfaces inside an oriented box.
// The owning entity's Transform defines the projection box: its world matrix orients
// the box and the box is additionally scaled by 'size'. The decal projects along the
// box local +Z axis.
#pragma once
#include <DirectXMath.h>
#include <string>
#include "Entity/Entity.h"

struct DecalComponent {
    std::string texturePath;                        // decal albedo texture (RGBA)
    DirectX::XMFLOAT3 size = { 1.0f, 1.0f, 1.0f };   // projection box full extents
    DirectX::XMFLOAT3 tint = { 1.0f, 1.0f, 1.0f };   // color multiplier
    float opacity = 1.0f;                            // overall blend strength [0,1]
    float angleFade = 0.0f;                          // 0 = project on all surfaces in box;
                                                     // >0 culls surfaces not facing +Z

    // When set to a valid entity, the decal box tracks that entity's world position
    // each frame (the decal's own Transform position acts as a local offset). Keeps the
    // decal's own rotation/scale. 0 = stationary.
    EntityID followTarget = 0;
};
