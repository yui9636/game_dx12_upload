#pragma once
#include <DirectXMath.h>
#include "Entity/Entity.h"

struct TransformComponent {
    // --- ローカル空間（エディタで直接いじる数値） ---
    DirectX::XMFLOAT3 localPosition = { 0, 0, 0 };
    DirectX::XMFLOAT4 localRotation = { 0, 0, 0, 1 };
    DirectX::XMFLOAT3 localScale = { 1, 1, 1 };

    // --- 親子関係 ---
    EntityID parent = 0;

    // --- 計算済みのキャッシュ（他のシステムが参照する） ---
    DirectX::XMFLOAT4X4 localMatrix; // ★追加：エラー解決のため
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 prevWorldMatrix;

    // ★既存システム（Collision等）が参照している変数名を「キャッシュ」として追加
    DirectX::XMFLOAT3 worldPosition = { 0, 0, 0 };
    DirectX::XMFLOAT4 worldRotation = { 0, 0, 0, 1 };
    DirectX::XMFLOAT3 worldScale = { 1, 1, 1 };

    // --- 進化ポイント：Dirtyフラグ ---
    bool isDirty = true;
};