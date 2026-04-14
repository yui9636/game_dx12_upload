#pragma once

#include <cstdint>

struct SequencerPreviewCameraComponent
{
    uint64_t bindingId = 0;
    bool hideFromHierarchy = true;
    bool hideFromInspector = true;
};
