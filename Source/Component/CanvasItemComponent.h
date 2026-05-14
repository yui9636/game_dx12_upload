// UI 要素の描画順、入力可否、スクリーン固定/ワールド配置の描画モードを保持する。
#pragma once

struct CanvasItemComponent
{
    int sortingLayer = 0;
    int orderInLayer = 0;
    bool visible = true;
    bool interactable = true;
    bool pixelSnap = false;
    bool lockAspect = false;
    bool screenSpaceOverlay = true;
};
