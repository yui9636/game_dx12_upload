#pragma once

struct CanvasItemComponent
{
    int sortingLayer = 0;
    int orderInLayer = 0;
    bool visible = true;
    bool interactable = true;
    bool pixelSnap = false;
    bool lockAspect = false;
};
