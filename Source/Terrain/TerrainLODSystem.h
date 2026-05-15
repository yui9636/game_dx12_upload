#pragma once

class Registry;
class TerrainBuildSystem;

// カメラ距離に基づき TerrainComponent の各チャンクの LOD レベルを更新する。
class TerrainLODSystem {
public:
    void Update(Registry& registry, TerrainBuildSystem& buildSystem);

    // LOD 距離閾値。距離が閾値を越えるたびに粗いインデックスバッファへ切り替える。
    float lodDistances[5] = { 64.0f, 128.0f, 256.0f, 512.0f, 1024.0f };
};
