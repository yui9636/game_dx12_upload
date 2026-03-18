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