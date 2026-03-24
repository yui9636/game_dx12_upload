#pragma once
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <unordered_map>
#include"JSONManager.h"

//==================================================
//==================================================

enum class RenderMode
{
    Billboard, 
    Mesh      
};

enum class ShapeType
{
    Point, Sphere, Box, Cone, Spark, Circle, Ring, Arc, Hemisphere, Ellipsoid, Torus, Line,Mesh
};
enum class PositionMode { Center, Random };
enum class LifeMode { Constant, Range };
enum class ScaleMode { Uniform, Range };

//==================================================
//==================================================
struct GradientColor { DirectX::XMFLOAT4 color; float time; };

//==================================================
//==================================================
struct ParticleSetting
{


    static constexpr int MaxGradientKeys = 4;

    bool  loop = true;
    float playSeconds = 5.0f;
    int   count = 200;
    bool  burst = true;
    int   burstFactor = 5;
    float spawnRate = 2.0f;


    RenderMode renderMode = RenderMode::Billboard;
    ShapeType           shape = ShapeType::Sphere;
    DirectX::XMFLOAT3   position{ 0,0,0 };
    float               radius = 0.3f;
    DirectX::XMFLOAT3   boxSize{ 1,1,1 };

    DirectX::XMFLOAT3   coneDirection{ 0,1,0 };
    float               coneAngleDeg = 30.0f;
    float               minSpeed = 1.0f;
    float               maxSpeed = 5.0f;

    DirectX::XMFLOAT3   minVelocity{ 0,0,0 };
    DirectX::XMFLOAT3   maxVelocity{ 0,0,0 };
    DirectX::XMFLOAT3   acceleration{ 0,0,0 };
    bool                useGravity = false;
    float               gravityPower = 9.8f;
    DirectX::XMFLOAT3   gravityDirection{ 0,-1,0 };

    LifeMode            lifeMode = LifeMode::Constant;
    float               lifeSeconds = 3.0f;
    float               lifeMin = 1.0f;
    float               lifeMax = 3.0f;

    ScaleMode           scaleMode = ScaleMode::Uniform;
    DirectX::XMFLOAT2   scale{ 0.2f, 0.28f };
    DirectX::XMFLOAT2   scaleBeginRange{ 0.2f, 0.28f };
    DirectX::XMFLOAT2   scaleEndRange{ 1.0f, 1.0f };

    DirectX::XMFLOAT2   angularVelocityRangeZ{ -DirectX::XM_PIDIV4, DirectX::XM_PIDIV4 };

    DirectX::XMFLOAT4   color{ 1,1,1,1 };
    GradientColor       gradientColors[MaxGradientKeys]{
        {{1,0,0,1},0.0f}, {{1,1,0,1},0.33f}, {{0,1,0,1},0.66f}, {{0,0,1,1},1.0f}
    };
    int                 gradientCount = 1;
    float               fadeInRatio = 0.0f;
    float               fadeOutRatio = 0.0f;

    PositionMode        positionMode = PositionMode::Random;
    DirectX::XMFLOAT4X4 parentMatrix{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    bool                useLocalSpace = false;

    DirectX::XMUINT2    textureSplitCount{ 1,1 };
    int                 spriteIndex = 0;
    int                 spriteFrameCount = 1;
    float               spriteFPS = 16.0f;

    float               circleRadius = 1.0f;
    float               ringInnerRadius = 0.5f;
    float               ringOuterRadius = 1.0f;
    float               arcStartDegree = -45.0f;
    float               arcEndDegree = +45.0f;
    bool                arcFill = true;
    DirectX::XMFLOAT3   ellipsoidRadii{ 0.7f, 1.0f, 0.7f };
    float               torusMajorRadius = 1.0f;
    float               torusMinorRadius = 0.25f;
    DirectX::XMFLOAT3   linePointA{ -0.5f, 0.0f, 0.0f };
    DirectX::XMFLOAT3   linePointB{ +0.5f, 0.0f, 0.0f };
    bool                surfaceOnly = false;
};


//==================================================
// ParticleRendererSettings
//==================================================
struct ParticleRendererSettings
{
    bool  velocityStretchEnabled = false;
    float velocityStretchScale = 0.05f;
    float velocityStretchMaxAspect = 8.0f;
    float velocityStretchMinSpeed = 0.0f;

    float curlNoiseStrength = 0.0f;
    float curlNoiseScale = 0.1f;
    float curlMoveSpeed = 0.2f;
};

//==================================================
// nlohmann::json ADL serializers
//==================================================
namespace nlohmann {

    template<> struct adl_serializer<DirectX::XMUINT2> {
        static void to_json(json& j, const DirectX::XMUINT2& v) { j = { {"x", v.x}, {"y", v.y} }; }
        static void from_json(const json& j, DirectX::XMUINT2& v) { j.at("x").get_to(v.x); j.at("y").get_to(v.y); }
    };

    template<> struct adl_serializer<ShapeType> {
        static void to_json(json& j, const ShapeType& s) {
            switch (s) {
            case ShapeType::Point:      j = "Point";      break;
            case ShapeType::Sphere:     j = "Sphere";     break;
            case ShapeType::Box:        j = "Box";        break;
            case ShapeType::Cone:       j = "Cone";       break;
            case ShapeType::Spark:      j = "Spark";      break;
            case ShapeType::Circle:     j = "Circle";     break;
            case ShapeType::Ring:       j = "Ring";       break;
            case ShapeType::Arc:        j = "Arc";        break;
            case ShapeType::Hemisphere: j = "Hemisphere"; break;
            case ShapeType::Ellipsoid:  j = "Ellipsoid";  break;
            case ShapeType::Torus:      j = "Torus";      break;
            case ShapeType::Line:       j = "Line";       break;
            case ShapeType::Mesh:       j = "Mesh";       break;
            default:                    j = "Sphere";     break;
            }
        }
        static void from_json(const json& j, ShapeType& s) {
            const std::string v = j.get<std::string>();
            if (v == "Point")      s = ShapeType::Point;
            else if (v == "Sphere")     s = ShapeType::Sphere;
            else if (v == "Box")        s = ShapeType::Box;
            else if (v == "Cone")       s = ShapeType::Cone;
            else if (v == "Spark")      s = ShapeType::Spark;
            else if (v == "Circle")     s = ShapeType::Circle;
            else if (v == "Ring")       s = ShapeType::Ring;
            else if (v == "Arc")        s = ShapeType::Arc;
            else if (v == "Hemisphere") s = ShapeType::Hemisphere;
            else if (v == "Ellipsoid")  s = ShapeType::Ellipsoid;
            else if (v == "Torus")      s = ShapeType::Torus;
            else if (v == "Line")       s = ShapeType::Line;
            else if (v == "Mesh")       s = ShapeType::Mesh;
            else                        s = ShapeType::Sphere;
        }
    };

    template<> struct adl_serializer<LifeMode> {
        static void to_json(json& j, const LifeMode& v) { j = (v == LifeMode::Constant) ? "Constant" : "Range"; }
        static void from_json(const json& j, LifeMode& v) { v = (j.get<std::string>() == "Range") ? LifeMode::Range : LifeMode::Constant; }
    };
    template<> struct adl_serializer<ScaleMode> {
        static void to_json(json& j, const ScaleMode& v) { j = (v == ScaleMode::Uniform) ? "Uniform" : "Range"; }
        static void from_json(const json& j, ScaleMode& v) { v = (j.get<std::string>() == "Range") ? ScaleMode::Range : ScaleMode::Uniform; }
    };
    template<> struct adl_serializer<PositionMode> {
        static void to_json(json& j, const PositionMode& v) { j = (v == PositionMode::Center) ? "Center" : "Random"; }
        static void from_json(const json& j, PositionMode& v) { v = (j.get<std::string>() == "Center") ? PositionMode::Center : PositionMode::Random; }
    };

    template<> struct adl_serializer<ParticleSetting>
    {
        static void to_json(json& j, const ParticleSetting& e)
        {
            j = {
                {"loop", e.loop}, {"playSeconds", e.playSeconds},
                {"count", e.count}, {"burst", e.burst}, {"burstFactor", e.burstFactor}, {"spawnRate", e.spawnRate},

                {"shape", e.shape}, {"position", e.position}, {"radius", e.radius}, {"boxSize", e.boxSize},

                {"coneDirection", e.coneDirection}, {"coneAngleDeg", e.coneAngleDeg},
                {"minSpeed", e.minSpeed}, {"maxSpeed", e.maxSpeed},

                {"minVelocity", e.minVelocity}, {"maxVelocity", e.maxVelocity}, {"acceleration", e.acceleration},
                {"useGravity", e.useGravity}, {"gravityPower", e.gravityPower}, {"gravityDirection", e.gravityDirection},

                {"lifeMode", e.lifeMode}, {"lifeSeconds", e.lifeSeconds}, {"lifeMin", e.lifeMin}, {"lifeMax", e.lifeMax},

                {"scaleMode", e.scaleMode}, {"scale", e.scale},
                {"scaleBeginRange", e.scaleBeginRange}, {"scaleEndRange", e.scaleEndRange},

                {"angularVelocityRangeZ", e.angularVelocityRangeZ},

                {"color", e.color},
                {"gradientCount", e.gradientCount},
                {"gradientColors", [&] {
                    std::vector<json> arr;
                    for (int i = 0; i < e.gradientCount; ++i)
                        arr.push_back({{"color",e.gradientColors[i].color},{"time",e.gradientColors[i].time}});
                    return arr;
                }()},
                {"fadeInRatio", e.fadeInRatio}, {"fadeOutRatio", e.fadeOutRatio},

                {"positionMode", e.positionMode}, {"parentMatrix", e.parentMatrix}, {"useLocalSpace", e.useLocalSpace},

                {"textureSplitCount", e.textureSplitCount},
                {"spriteIndex", e.spriteIndex},
                {"spriteFrameCount", e.spriteFrameCount},
                {"spriteFPS", e.spriteFPS},


                {"circleRadius",      e.circleRadius},
                {"ringInnerRadius",   e.ringInnerRadius},
                {"ringOuterRadius",   e.ringOuterRadius},
                {"arcStartDegree",    e.arcStartDegree},
                {"arcEndDegree",      e.arcEndDegree},
                {"arcFill",           e.arcFill},
                {"ellipsoidRadii",    e.ellipsoidRadii},
                {"torusMajorRadius",  e.torusMajorRadius},
                {"torusMinorRadius",  e.torusMinorRadius},
                {"linePointA",        e.linePointA},
                {"linePointB",        e.linePointB},
                {"surfaceOnly",       e.surfaceOnly}



            };
        }

        static void from_json(const json& j, ParticleSetting& e)
        {
            if (j.contains("loop"))        j.at("loop").get_to(e.loop);
            if (j.contains("playSeconds")) j.at("playSeconds").get_to(e.playSeconds);
            if (j.contains("count"))       j.at("count").get_to(e.count);
            if (j.contains("burst"))       j.at("burst").get_to(e.burst);
            if (j.contains("burstFactor")) j.at("burstFactor").get_to(e.burstFactor);
            if (j.contains("spawnRate"))   j.at("spawnRate").get_to(e.spawnRate);

            if (j.contains("shape"))    j.at("shape").get_to(e.shape);
            if (j.contains("position")) j.at("position").get_to(e.position);
            if (j.contains("radius"))   j.at("radius").get_to(e.radius);
            if (j.contains("boxSize"))  j.at("boxSize").get_to(e.boxSize);

            if (j.contains("coneDirection")) j.at("coneDirection").get_to(e.coneDirection);
            if (j.contains("coneAngleDeg"))  j.at("coneAngleDeg").get_to(e.coneAngleDeg);
            if (j.contains("minSpeed"))      j.at("minSpeed").get_to(e.minSpeed);
            if (j.contains("maxSpeed"))      j.at("maxSpeed").get_to(e.maxSpeed);

            if (j.contains("minVelocity"))   j.at("minVelocity").get_to(e.minVelocity);
            if (j.contains("maxVelocity"))   j.at("maxVelocity").get_to(e.maxVelocity);
            if (j.contains("acceleration"))  j.at("acceleration").get_to(e.acceleration);
            if (j.contains("useGravity"))    j.at("useGravity").get_to(e.useGravity);
            if (j.contains("gravityPower"))  j.at("gravityPower").get_to(e.gravityPower);
            if (j.contains("gravityDirection")) j.at("gravityDirection").get_to(e.gravityDirection);

            if (j.contains("lifeMode"))    j.at("lifeMode").get_to(e.lifeMode);
            if (j.contains("lifeSeconds")) j.at("lifeSeconds").get_to(e.lifeSeconds);
            if (j.contains("lifeMin"))     j.at("lifeMin").get_to(e.lifeMin);
            if (j.contains("lifeMax"))     j.at("lifeMax").get_to(e.lifeMax);

            if (j.contains("scaleMode"))       j.at("scaleMode").get_to(e.scaleMode);
            if (j.contains("scale"))           j.at("scale").get_to(e.scale);
            if (j.contains("scaleBeginRange")) j.at("scaleBeginRange").get_to(e.scaleBeginRange);
            if (j.contains("scaleEndRange"))   j.at("scaleEndRange").get_to(e.scaleEndRange);

            if (j.contains("angularVelocityRangeZ")) j.at("angularVelocityRangeZ").get_to(e.angularVelocityRangeZ);

            if (j.contains("color"))           j.at("color").get_to(e.color);
            if (j.contains("gradientCount"))   j.at("gradientCount").get_to(e.gradientCount);
            if (j.contains("gradientColors")) {
                const auto& arr = j["gradientColors"];
                for (int i = 0; i < e.gradientCount && i < ParticleSetting::MaxGradientKeys; ++i) {
                    e.gradientColors[i].color = arr[i]["color"].get<DirectX::XMFLOAT4>();
                    e.gradientColors[i].time = arr[i]["time"].get<float>();
                }
            }
            if (j.contains("fadeInRatio"))  j.at("fadeInRatio").get_to(e.fadeInRatio);
            if (j.contains("fadeOutRatio")) j.at("fadeOutRatio").get_to(e.fadeOutRatio);

            if (j.contains("positionMode"))  j.at("positionMode").get_to(e.positionMode);
            if (j.contains("parentMatrix"))  j.at("parentMatrix").get_to(e.parentMatrix);
            if (j.contains("useLocalSpace")) j.at("useLocalSpace").get_to(e.useLocalSpace);

            if (j.contains("textureSplitCount")) j.at("textureSplitCount").get_to(e.textureSplitCount);
            if (j.contains("spriteIndex"))       j.at("spriteIndex").get_to(e.spriteIndex);
            if (j.contains("spriteFrameCount"))  j.at("spriteFrameCount").get_to(e.spriteFrameCount);
            if (j.contains("spriteFPS"))         j.at("spriteFPS").get_to(e.spriteFPS);

            if (j.contains("circleRadius"))     j.at("circleRadius").get_to(e.circleRadius);
            if (j.contains("ringInnerRadius"))  j.at("ringInnerRadius").get_to(e.ringInnerRadius);
            if (j.contains("ringOuterRadius"))  j.at("ringOuterRadius").get_to(e.ringOuterRadius);
            if (j.contains("arcStartDegree"))   j.at("arcStartDegree").get_to(e.arcStartDegree);
            if (j.contains("arcEndDegree"))     j.at("arcEndDegree").get_to(e.arcEndDegree);
            if (j.contains("arcFill"))          j.at("arcFill").get_to(e.arcFill);
            if (j.contains("ellipsoidRadii"))   j.at("ellipsoidRadii").get_to(e.ellipsoidRadii);
            if (j.contains("torusMajorRadius")) j.at("torusMajorRadius").get_to(e.torusMajorRadius);
            if (j.contains("torusMinorRadius")) j.at("torusMinorRadius").get_to(e.torusMinorRadius);
            if (j.contains("linePointA"))       j.at("linePointA").get_to(e.linePointA);
            if (j.contains("linePointB"))       j.at("linePointB").get_to(e.linePointB);
            if (j.contains("surfaceOnly"))      j.at("surfaceOnly").get_to(e.surfaceOnly);
        }
    };

    template<> struct adl_serializer<ParticleRendererSettings>
    {
        static void to_json(json& j, const ParticleRendererSettings& p)
        {
            j = {
                {"velocityStretchEnabled",   p.velocityStretchEnabled},
                {"velocityStretchScale",     p.velocityStretchScale},
                {"velocityStretchMaxAspect", p.velocityStretchMaxAspect},
                {"velocityStretchMinSpeed",  p.velocityStretchMinSpeed},

                {"curlNoiseStrength",        p.curlNoiseStrength},
                {"curlNoiseScale",           p.curlNoiseScale},
                {"curlMoveSpeed",            p.curlMoveSpeed}
            };
        }

        static void from_json(const json& j, ParticleRendererSettings& p)
        {
            if (j.contains("velocityStretchEnabled"))   j.at("velocityStretchEnabled").get_to(p.velocityStretchEnabled);
            if (j.contains("velocityStretchScale"))     j.at("velocityStretchScale").get_to(p.velocityStretchScale);
            if (j.contains("velocityStretchMaxAspect")) j.at("velocityStretchMaxAspect").get_to(p.velocityStretchMaxAspect);
            if (j.contains("velocityStretchMinSpeed"))  j.at("velocityStretchMinSpeed").get_to(p.velocityStretchMinSpeed);

            if (j.contains("curlNoiseStrength"))        j.at("curlNoiseStrength").get_to(p.curlNoiseStrength);
            if (j.contains("curlNoiseScale"))           j.at("curlNoiseScale").get_to(p.curlNoiseScale);
            if (j.contains("curlMoveSpeed"))            j.at("curlMoveSpeed").get_to(p.curlMoveSpeed);
        }
    };

} // namespace nlohmann

