#pragma once
// Mathf はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

class Mathf
{
public:
    static float Lerp(float a, float b, float t);

    static float RandomRange(float min, float max);
};
