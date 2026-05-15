#pragma once

class Registry;

// カメラ距離に基づき TerrainComponent の各チャンクの LOD レベルを更新する。
// 現フェーズでは可視/非可視フラグのみ管理する。
class TerrainLODSystem {
public:
    void Update(Registry& registry);

    // LOD 距離閾値 (メートル単位)。インデックス = LOD レベル。
    float lodDistances[5] = { 64.0f, 128.0f, 256.0f, 512.0f, 1024.0f };
};
