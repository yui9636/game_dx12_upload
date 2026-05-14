// IShader の RHI 関連インターフェースまたは実装宣言をまとめます。
#pragma once

enum class ShaderType {
    Vertex,
    Pixel,
    Compute
};

class IShader {
public:
    virtual ~IShader() = default;
    virtual ShaderType GetType() const = 0;

};