#pragma once

// Tag marking the active Camera2D for 2D rendering.
// Symmetric to CameraMainTagComponent on the 3D side.
// If multiple instances exist, the first one found wins and a warning is logged.
struct Camera2DMainTagComponent {};
