#pragma once

class Registry;
// TimelineAsset のデータを実行時に TimelineItemBuffer へ変換する。
// エディタで作成したタイムラインデータを既存の Gameplay システムへ橋渡しする。
class TimelineRuntimeSystem
{
public:
    static void Update(Registry& registry, float dt);
};
