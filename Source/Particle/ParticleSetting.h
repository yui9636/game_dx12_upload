#pragma once
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <unordered_map>
#include"JSONManager.h"

//==================================================
// パーティクル描画方式
//==================================================

// パーティクルをどの見た目で描画するかを表す列挙型。
enum class RenderMode
{
    // カメラ方向を向く板ポリゴンとして描画する。
    Billboard, 

    // メッシュ形状として描画する。
    Mesh      
};

// パーティクルの発生形状を表す列挙型。
enum class ShapeType
{
    // 1点から発生させる。
    Point,

    // 球状に発生させる。
    Sphere,

    // 箱状に発生させる。
    Box,

    // 円錐方向に発生させる。
    Cone,

    // 火花のような方向付き発生に使う。
    Spark,

    // 円形に発生させる。
    Circle,

    // リング状に発生させる。
    Ring,

    // 扇形に発生させる。
    Arc,

    // 半球状に発生させる。
    Hemisphere,

    // 楕円体状に発生させる。
    Ellipsoid,

    // トーラス状に発生させる。
    Torus,

    // 線分上に発生させる。
    Line,

    // メッシュ表面またはメッシュ由来の形状から発生させる。
    Mesh
};

// 発生位置の決め方。
enum class PositionMode
{
    // 中心位置から発生させる。
    Center,

    // 形状内または形状表面のランダム位置から発生させる。
    Random
};

// 寿命の決め方。
enum class LifeMode
{
    // 全パーティクルで一定寿命を使う。
    Constant,

    // 最小寿命から最大寿命の範囲でランダムに決める。
    Range
};

// スケールの決め方。
enum class ScaleMode
{
    // 一定のスケールを使う。
    Uniform,

    // 開始スケールと終了スケールを範囲指定する。
    Range
};

//==================================================
// グラデーション色キー
//==================================================

// 指定時刻における色を保持する構造体。
struct GradientColor
{
    // このキーで使う色。
    DirectX::XMFLOAT4 color;

    // 0.0～1.0 の正規化時間。
    float time;
};

//==================================================
// パーティクル発生・挙動設定
//==================================================
struct ParticleSetting
{
    // グラデーションに登録できる最大キー数。
    static constexpr int MaxGradientKeys = 4;

    // 再生終了後にループするかどうか。
    bool  loop = true;

    // 1回の再生時間。
    float playSeconds = 5.0f;

    // 管理するパーティクル数。
    int   count = 200;

    // 初回にまとめて発生させるかどうか。
    bool  burst = true;

    // バースト時の発生倍率。
    int   burstFactor = 5;

    // 1秒あたりの通常発生数。
    float spawnRate = 2.0f;

    // 描画モード。
    RenderMode renderMode = RenderMode::Billboard;

    // 発生形状。
    ShapeType           shape = ShapeType::Sphere;

    // エミッタの基準位置。
    DirectX::XMFLOAT3   position{ 0,0,0 };

    // 球・円などで使う半径。
    float               radius = 0.3f;

    // 箱形状で使う大きさ。
    DirectX::XMFLOAT3   boxSize{ 1,1,1 };

    // Cone / Spark の基準方向。
    DirectX::XMFLOAT3   coneDirection{ 0,1,0 };

    // Cone / Spark の広がり角度。
    float               coneAngleDeg = 30.0f;

    // ランダム速度の最小値。
    float               minSpeed = 1.0f;

    // ランダム速度の最大値。
    float               maxSpeed = 5.0f;

    // 速度を成分ごとに指定する場合の最小値。
    DirectX::XMFLOAT3   minVelocity{ 0,0,0 };

    // 速度を成分ごとに指定する場合の最大値。
    DirectX::XMFLOAT3   maxVelocity{ 0,0,0 };

    // 毎フレーム加える加速度。
    DirectX::XMFLOAT3   acceleration{ 0,0,0 };

    // 重力を使うかどうか。
    bool                useGravity = false;

    // 重力の強さ。
    float               gravityPower = 9.8f;

    // 重力の方向。
    DirectX::XMFLOAT3   gravityDirection{ 0,-1,0 };

    // 寿命の決定方式。
    LifeMode            lifeMode = LifeMode::Constant;

    // 一定寿命モードで使う寿命秒数。
    float               lifeSeconds = 3.0f;

    // ランダム寿命の最小値。
    float               lifeMin = 1.0f;

    // ランダム寿命の最大値。
    float               lifeMax = 3.0f;

    // スケールの決定方式。
    ScaleMode           scaleMode = ScaleMode::Uniform;

    // 一定スケールで使う開始・終了スケール。
    DirectX::XMFLOAT2   scale{ 0.2f, 0.28f };

    // ランダムスケールの開始範囲。
    DirectX::XMFLOAT2   scaleBeginRange{ 0.2f, 0.28f };

    // ランダムスケールの終了範囲。
    DirectX::XMFLOAT2   scaleEndRange{ 1.0f, 1.0f };

    // Z軸回転速度のランダム範囲。
    DirectX::XMFLOAT2   angularVelocityRangeZ{ -DirectX::XM_PIDIV4, DirectX::XM_PIDIV4 };

    // 基本色。
    DirectX::XMFLOAT4   color{ 1,1,1,1 };

    // 時間に応じて色を変えるためのグラデーションキー。
    GradientColor       gradientColors[MaxGradientKeys]{
        {{1,0,0,1},0.0f}, {{1,1,0,1},0.33f}, {{0,1,0,1},0.66f}, {{0,0,1,1},1.0f}
    };

    // 使用するグラデーションキー数。
    int                 gradientCount = 1;

    // 寿命序盤でフェードインする割合。
    float               fadeInRatio = 0.0f;

    // 寿命終盤でフェードアウトする割合。
    float               fadeOutRatio = 0.0f;

    // 発生位置の決定方式。
    PositionMode        positionMode = PositionMode::Random;

    // 親空間に追従させるための親行列。
    DirectX::XMFLOAT4X4 parentMatrix{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

    // ローカル空間で動かすかどうか。
    bool                useLocalSpace = false;

    // テクスチャを何分割してスプライトアニメーションに使うか。
    DirectX::XMUINT2    textureSplitCount{ 1,1 };

    // 使用するスプライト番号。
    int                 spriteIndex = 0;

    // スプライトアニメーションのフレーム数。
    int                 spriteFrameCount = 1;

    // スプライトアニメーションの再生速度。
    float               spriteFPS = 16.0f;

    // 円形発生で使う半径。
    float               circleRadius = 1.0f;

    // リング発生の内側半径。
    float               ringInnerRadius = 0.5f;

    // リング発生の外側半径。
    float               ringOuterRadius = 1.0f;

    // 扇形発生の開始角度。
    float               arcStartDegree = -45.0f;

    // 扇形発生の終了角度。
    float               arcEndDegree = +45.0f;

    // 扇形の内部も埋めて発生させるかどうか。
    bool                arcFill = true;

    // 楕円体発生で使う各軸半径。
    DirectX::XMFLOAT3   ellipsoidRadii{ 0.7f, 1.0f, 0.7f };

    // トーラス発生の主半径。
    float               torusMajorRadius = 1.0f;

    // トーラス発生の管半径。
    float               torusMinorRadius = 0.25f;

    // 線分発生の開始点。
    DirectX::XMFLOAT3   linePointA{ -0.5f, 0.0f, 0.0f };

    // 線分発生の終了点。
    DirectX::XMFLOAT3   linePointB{ +0.5f, 0.0f, 0.0f };

    // true の場合は形状内部ではなく表面のみから発生させる。
    bool                surfaceOnly = false;
};

//==================================================
// パーティクル描画補助設定
//==================================================
struct ParticleRendererSettings
{
    // 速度方向にパーティクルを伸ばすかどうか。
    bool  velocityStretchEnabled = false;

    // 速度伸ばしの倍率。
    float velocityStretchScale = 0.05f;

    // 速度伸ばし時の最大アスペクト比。
    float velocityStretchMaxAspect = 8.0f;

    // この速度未満では速度伸ばしを行わない。
    float velocityStretchMinSpeed = 0.0f;

    // カールノイズによる揺らぎの強さ。
    float curlNoiseStrength = 0.0f;

    // カールノイズの空間スケール。
    float curlNoiseScale = 0.1f;

    // カールノイズを時間方向に動かす速度。
    float curlMoveSpeed = 0.2f;
};

//==================================================
// nlohmann::json 用の変換定義
//==================================================
namespace nlohmann {

    // XMUINT2 を JSON の x/y 形式に変換する。
    template<> struct adl_serializer<DirectX::XMUINT2> {
        static void to_json(json& j, const DirectX::XMUINT2& v) { j = { {"x", v.x}, {"y", v.y} }; }
        static void from_json(const json& j, DirectX::XMUINT2& v) { j.at("x").get_to(v.x); j.at("y").get_to(v.y); }
    };

    // ShapeType を文字列として保存・復元する。
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

    // LifeMode を文字列として保存・復元する。
    template<> struct adl_serializer<LifeMode> {
        static void to_json(json& j, const LifeMode& v) { j = (v == LifeMode::Constant) ? "Constant" : "Range"; }
        static void from_json(const json& j, LifeMode& v) { v = (j.get<std::string>() == "Range") ? LifeMode::Range : LifeMode::Constant; }
    };

    // ScaleMode を文字列として保存・復元する。
    template<> struct adl_serializer<ScaleMode> {
        static void to_json(json& j, const ScaleMode& v) { j = (v == ScaleMode::Uniform) ? "Uniform" : "Range"; }
        static void from_json(const json& j, ScaleMode& v) { v = (j.get<std::string>() == "Range") ? ScaleMode::Range : ScaleMode::Uniform; }
    };

    // PositionMode を文字列として保存・復元する。
    template<> struct adl_serializer<PositionMode> {
        static void to_json(json& j, const PositionMode& v) { j = (v == PositionMode::Center) ? "Center" : "Random"; }
        static void from_json(const json& j, PositionMode& v) { v = (j.get<std::string>() == "Center") ? PositionMode::Center : PositionMode::Random; }
    };

    // ParticleSetting 全体を JSON に保存・復元する。
    template<> struct adl_serializer<ParticleSetting>
    {
        // ParticleSetting の各項目を JSON オブジェクトへ書き出す。
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

        // JSON に存在する項目だけを ParticleSetting に読み戻す。
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

    // ParticleRendererSettings 全体を JSON に保存・復元する。
    template<> struct adl_serializer<ParticleRendererSettings>
    {
        // 描画補助設定を JSON に書き出す。
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

        // JSON に存在する項目だけを描画補助設定へ読み戻す。
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
