//#include "EffectLoader.h"
//#include "EffectNode.h"
//#include "Effect/MeshEmitter.h"
//#include "Particle/ParticleEmitter.h"
//#include "EffectMaterial.h"
//#include "EffectManager.h"
//#include "Graphics.h"
//#include "JSONManager.h" // DirectX::XMFLOAT系のシリアライズを利用
//#include <fstream>
//#include <iostream>
//
//using json = nlohmann::json;
//using namespace DirectX;
//
//// =================================================================
//// 内部ヘルパー: 追加のJSONシリアライザ定義
//// (ヘッダを変更せずにローダー内で完結させるための処置)
//// =================================================================
//namespace nlohmann {
//
//    // EffectMaterialConstants の変換
//    template<> struct adl_serializer<EffectMaterialConstants> {
//        static void to_json(json& j, const EffectMaterialConstants& c) {
//            j = json{
//                {"baseColor", {c.baseColor.x, c.baseColor.y, c.baseColor.z, c.baseColor.w}},
//                {"emissiveIntensity", c.emissiveIntensity},
//                {"mainUvScrollSpeed", {c.mainUvScrollSpeed.x, c.mainUvScrollSpeed.y}},
//                {"distortionStrength", c.distortionStrength},
//                {"maskEdgeFade", c.maskEdgeFade},
//                {"distortionUvScrollSpeed", {c.distortionUvScrollSpeed.x, c.distortionUvScrollSpeed.y}},
//                {"dissolveThreshold", c.dissolveThreshold},
//                {"dissolveEdgeWidth", c.dissolveEdgeWidth},
//                {"maskIntensity", c.maskIntensity},
//                {"maskContrast", c.maskContrast},
//                {"dissolveEdgeColor", {c.dissolveEdgeColor.x, c.dissolveEdgeColor.y, c.dissolveEdgeColor.z}},
//                {"mainTexIndex", c.mainTexIndex},
//                {"distortionTexIndex", c.distortionTexIndex},
//                {"dissolveTexIndex", c.dissolveTexIndex},
//                {"maskTexIndex", c.maskTexIndex},
//                {"fresnelColor", {c.fresnelColor.x, c.fresnelColor.y, c.fresnelColor.z}},
//                {"fresnelPower", c.fresnelPower},
//                {"flipbookWidth", c.flipbookWidth},
//                {"flipbookHeight", c.flipbookHeight},
//                {"flipbookSpeed", c.flipbookSpeed},
//                {"gradientStrength", c.gradientStrength},
//                {"gradientTexIndex", c.gradientTexIndex},
//                {"wpoStrength", c.wpoStrength},
//                {"wpoSpeed", c.wpoSpeed},
//                {"wpoFrequency", c.wpoFrequency},
//                {"chromaticAberrationStrength", c.chromaticAberrationStrength},
//                {"dissolveGlowIntensity", c.dissolveGlowIntensity},
//                {"dissolveGlowRange", c.dissolveGlowRange},
//                {"uvScrollMode", c.uvScrollMode},
//                {"dissolveGlowColor", {c.dissolveGlowColor.x, c.dissolveGlowColor.y, c.dissolveGlowColor.z}},
//                {"matCapTexIndex", c.matCapTexIndex},
//                {"matCapStrength", c.matCapStrength},
//                {"matCapBlend", c.matCapBlend},
//                {"clipSoftness", c.clipSoftness},           
//                {"startEndFadeWidth", c.startEndFadeWidth},
//                {"matCapColor", {c.matCapColor.x, c.matCapColor.y, c.matCapColor.z}},
//                {"normalTexIndex", c.normalTexIndex},
//                {"normalStrength", c.normalStrength},
//                {"flowTexIndex", c.flowTexIndex},
//                {"flowStrength", c.flowStrength},
//                {"flowSpeed", c.flowSpeed},
//                {"sideFadeWidth", c.sideFadeWidth},
//                {"visibility", c.visibility},
//                {"clipStart", c.clipStart},
//                {"clipEnd", c.clipEnd},
//                {"subTexIndex", c.subTexIndex},
//                {"subBlendMode", c.subBlendMode},
//                {"subUvScrollSpeed", {c.subUvScrollSpeed.x, c.subUvScrollSpeed.y}},
//                {"subTexStrength", c.subTexStrength},
//                {"subUvRotationSpeed", c.subUvRotationSpeed},
//                {"usePolarCoords", c.usePolarCoords},
//                {"toonThreshold", c.toonThreshold},
//                {"toonSmoothing", c.toonSmoothing},
//                {"toonSteps", c.toonSteps},
//                {"toonNoiseStrength", c.toonNoiseStrength},
//                {"toonNoiseSpeed", c.toonNoiseSpeed},
//                {"toonShadowColor", {c.toonShadowColor.x, c.toonShadowColor.y, c.toonShadowColor.z, c.toonShadowColor.w}},
//                {"toonNoiseTexIndex", c.toonNoiseTexIndex},
//                {"toonSpecular", {c.toonSpecular.x, c.toonSpecular.y, c.toonSpecular.z, c.toonSpecular.w}},
//                {"toonRampTexIndex", c.toonRampTexIndex}
//            };
//        }
//        static void from_json(const json& j, EffectMaterialConstants& c) {
//            auto get_val = [&](const char* k, auto& d) { if (j.contains(k)) j.at(k).get_to(d); };
//            auto get_vec2 = [&](const char* k, DirectX::XMFLOAT2& d) { if (j.contains(k)) { auto v = j.at(k); d = { v[0], v[1] }; } };
//            auto get_vec3 = [&](const char* k, DirectX::XMFLOAT3& d) { if (j.contains(k)) { auto v = j.at(k); d = { v[0], v[1], v[2] }; } };
//            auto get_vec4 = [&](const char* k, DirectX::XMFLOAT4& d) { if (j.contains(k)) { auto v = j.at(k); d = { v[0], v[1], v[2], v[3] }; } };
//
//            get_vec4("baseColor", c.baseColor);
//            get_val("emissiveIntensity", c.emissiveIntensity);
//            get_vec2("mainUvScrollSpeed", c.mainUvScrollSpeed);
//            get_val("distortionStrength", c.distortionStrength);
//            get_val("maskEdgeFade", c.maskEdgeFade);
//            get_vec2("distortionUvScrollSpeed", c.distortionUvScrollSpeed);
//            get_val("dissolveThreshold", c.dissolveThreshold);
//            get_val("dissolveEdgeWidth", c.dissolveEdgeWidth);
//            get_val("maskIntensity", c.maskIntensity);
//            get_val("maskContrast", c.maskContrast);
//            get_vec3("dissolveEdgeColor", c.dissolveEdgeColor);
//            get_val("mainTexIndex", c.mainTexIndex);
//            get_val("distortionTexIndex", c.distortionTexIndex);
//            get_val("dissolveTexIndex", c.dissolveTexIndex);
//            get_val("maskTexIndex", c.maskTexIndex);
//            get_vec3("fresnelColor", c.fresnelColor);
//            get_val("fresnelPower", c.fresnelPower);
//            get_val("flipbookWidth", c.flipbookWidth);
//            get_val("flipbookHeight", c.flipbookHeight);
//            get_val("flipbookSpeed", c.flipbookSpeed);
//            get_val("gradientStrength", c.gradientStrength);
//            get_val("gradientTexIndex", c.gradientTexIndex);
//            get_val("wpoStrength", c.wpoStrength);
//            get_val("wpoSpeed", c.wpoSpeed);
//            get_val("wpoFrequency", c.wpoFrequency);
//            get_val("chromaticAberrationStrength", c.chromaticAberrationStrength);
//            get_val("dissolveGlowIntensity", c.dissolveGlowIntensity);
//            get_val("dissolveGlowRange", c.dissolveGlowRange);
//            get_val("uvScrollMode", c.uvScrollMode);
//            get_vec3("dissolveGlowColor", c.dissolveGlowColor);
//            get_val("matCapTexIndex", c.matCapTexIndex);
//            get_val("matCapStrength", c.matCapStrength);
//            get_val("matCapBlend", c.matCapBlend);
//            get_val("clipSoftness", c.clipSoftness);
//            get_val("startEndFadeWidth", c.startEndFadeWidth);
//            get_vec3("matCapColor", c.matCapColor);
//            get_val("normalTexIndex", c.normalTexIndex);
//            get_val("normalStrength", c.normalStrength);
//            get_val("flowTexIndex", c.flowTexIndex);
//            get_val("flowStrength", c.flowStrength);
//            get_val("flowSpeed", c.flowSpeed);
//            get_val("sideFadeWidth", c.sideFadeWidth);
//            get_val("visibility", c.visibility);
//            get_val("clipStart", c.clipStart);
//            get_val("clipEnd", c.clipEnd);
//            get_val("subTexIndex", c.subTexIndex);
//            get_val("subBlendMode", c.subBlendMode);
//            get_vec2("subUvScrollSpeed", c.subUvScrollSpeed);
//            get_val("subTexStrength", c.subTexStrength);
//            get_val("subUvRotationSpeed", c.subUvRotationSpeed);
//            get_val("usePolarCoords", c.usePolarCoords);
//            get_val("toonThreshold", c.toonThreshold);
//            get_val("toonSmoothing", c.toonSmoothing);
//            get_val("toonSteps", c.toonSteps);
//            get_val("toonNoiseStrength", c.toonNoiseStrength);
//            get_val("toonNoiseSpeed", c.toonNoiseSpeed);
//            get_vec4("toonShadowColor", c.toonShadowColor);
//            get_val("toonNoiseTexIndex", c.toonNoiseTexIndex);
//            get_vec4("toonSpecular", c.toonSpecular);
//            get_val("toonRampTexIndex", c.toonRampTexIndex);
//        }
//    };
//}
//
//// =================================================================
//// ヘルパー関数
//// =================================================================
//
//// カーブのロード
//void ParseCurve(const json& j, EffectCurve& curve)
//{
//    curve.keys.clear();
//    if (j.is_array())
//    {
//        for (const auto& keyJson : j)
//        {
//            float t = keyJson.value("time", 0.0f);
//            float v = keyJson.value("value", 0.0f);
//            curve.AddKey(t, v);
//        }
//    }
//}
//
//// カーブのセーブ
//json SerializeCurve(const EffectCurve& curve)
//{
//    json jArr = json::array();
//    for (const auto& k : curve.keys)
//    {
//        jArr.push_back({ {"time", k.time}, {"value", k.value} });
//    }
//    return jArr;
//}
//
//// マテリアルのロード
//void ParseMaterial(const json& jMat, MeshEmitter* meshNode)
//{
//    if (!meshNode) return;
//
//    // マテリアルがなければ作成
//    if (!meshNode->material) meshNode->material = std::make_shared<EffectMaterial>();
//    auto mat = meshNode->material;
//
//    // 1. 定数バッファ (EffectMaterialConstants)
//    if (jMat.contains("Constants")) {
//        jMat.at("Constants").get_to(mat->GetConstants());
//    }
//
//    // 2. ブレンドモード
//    if (jMat.contains("BlendMode")) {
//        mat->SetBlendMode((EffectBlendMode)jMat.value("BlendMode", 0));
//    }
//
//    // 3. テクスチャ
//    if (jMat.contains("Textures") && jMat["Textures"].is_array()) {
//        for (const auto& texJson : jMat["Textures"]) {
//            int slot = texJson.value("Slot", -1);
//            std::string path = texJson.value("Path", "");
//            if (slot >= 0 && !path.empty()) {
//                auto texture = EffectManager::Get().GetTexture(path);
//                mat->SetTexture(slot, path, texture);
//            }
//        }
//    }
//}
//
//// マテリアルのセーブ
//json SerializeMaterial(const std::shared_ptr<EffectMaterial>& mat)
//{
//    json jMat;
//    if (!mat) return jMat;
//
//    jMat["Constants"] = mat->GetConstants();
//    jMat["BlendMode"] = (int)mat->GetBlendMode();
//
//    json jTexs = json::array();
//    for (int i = 0; i < EffectMaterial::TEXTURE_SLOT_COUNT; ++i) {
//        std::string path = mat->GetTexturePath(i);
//        if (!path.empty()) {
//            jTexs.push_back({ {"Slot", i}, {"Path", path} });
//        }
//    }
//    jMat["Textures"] = jTexs;
//    return jMat;
//}
//
//
//std::shared_ptr<EffectNode> EffectLoader::LoadEffect(
//    const std::string& filePath,
//    float* outLife,
//    float* outFadeIn,
//    float* outFadeOut,
//    bool* outLoop)
//{
//    std::ifstream file(filePath);
//    if (!file.is_open()) return nullptr;
//
//    json j;
//    try {
//        file >> j;
//    }
//    catch (const std::exception& e) {
//        OutputDebugStringA(("JSON Parse Error: " + std::string(e.what()) + "\n").c_str());
//        return nullptr;
//    }
//
//    // ★追加: Settingsブロックの読み込み
//    if (j.contains("Settings")) {
//        auto& s = j["Settings"];
//        if (outLife)    *outLife = s.value("LifeTime", 2.0f);
//        if (outFadeIn)  *outFadeIn = s.value("FadeIn", 0.0f);
//        if (outFadeOut) *outFadeOut = s.value("FadeOut", 0.0f);
//        if (outLoop)    *outLoop = s.value("Loop", false);
//    }
//
//    // ★追加: Rootノードの読み込み分岐
//    // 新フォーマット: "Root" キーがある
//    if (j.contains("Root")) {
//        return ParseNode(j["Root"]);
//    }
//    // 旧フォーマット対応 (ファイル全体がノード)
//    else {
//        return ParseNode(j);
//    }
//}
//
//
//// =================================================================
//// ParseNode: ノードの再帰読み込み
//// =================================================================
//std::shared_ptr<EffectNode> EffectLoader::ParseNode(const json& j)
//{
//    std::shared_ptr<EffectNode> node = nullptr;
//    std::string type = j.value("Type", "Empty");
//
//    // --- Mesh Emitter ---
//    if (type == "MeshEmitter")
//    {
//        auto meshNode = std::make_shared<MeshEmitter>();
//        std::string modelPath = j.value("Model", "");
//        if (!modelPath.empty()) {
//            meshNode->LoadModel(modelPath);
//        }
//
//        if (j.contains("Material")) {
//            ParseMaterial(j["Material"], meshNode.get());
//        }
//
//        // カーブ読み込み
//        if (j.contains("Curves")) {
//            const auto& jc = j["Curves"];
//            if (jc.contains("ColorR")) ParseCurve(jc["ColorR"], meshNode->colorCurves[0]);
//            if (jc.contains("ColorG")) ParseCurve(jc["ColorG"], meshNode->colorCurves[1]);
//            if (jc.contains("ColorB")) ParseCurve(jc["ColorB"], meshNode->colorCurves[2]);
//            if (jc.contains("Visibility")) ParseCurve(jc["Visibility"], meshNode->visibilityCurve);
//            if (jc.contains("Emissive"))   ParseCurve(jc["Emissive"], meshNode->emissiveCurve);
//            if (jc.contains("MainUvScrollX")) ParseCurve(jc["MainUvScrollX"], meshNode->mainUvScrollCurves[0]);
//            if (jc.contains("MainUvScrollY")) ParseCurve(jc["MainUvScrollY"], meshNode->mainUvScrollCurves[1]);
//            if (jc.contains("WpoStrength")) ParseCurve(jc["WpoStrength"], meshNode->wpoStrengthCurve);
//            if (jc.contains("FresnelPower")) ParseCurve(jc["FresnelPower"], meshNode->fresnelPowerCurve);
//            if (jc.contains("DistortionStrength")) ParseCurve(jc["DistortionStrength"], meshNode->distortionStrengthCurve);
//            if (jc.contains("DistUvScrollX"))      ParseCurve(jc["DistUvScrollX"], meshNode->distUvScrollCurves[0]);
//            if (jc.contains("DistUvScrollY"))      ParseCurve(jc["DistUvScrollY"], meshNode->distUvScrollCurves[1]);
//            if (jc.contains("FlowStrength")) ParseCurve(jc["FlowStrength"], meshNode->flowStrengthCurve);
//            if (jc.contains("FlowSpeed"))    ParseCurve(jc["FlowSpeed"], meshNode->flowSpeedCurve);
//            if (jc.contains("Dissolve")) ParseCurve(jc["Dissolve"], meshNode->dissolveCurve);
//            if (jc.contains("ClipStart")) ParseCurve(jc["ClipStart"], meshNode->clipStartCurve);
//            if (jc.contains("ClipEnd"))   ParseCurve(jc["ClipEnd"], meshNode->clipEndCurve);
//            if (jc.contains("ClipSoftness")) ParseCurve(jc["ClipSoftness"], meshNode->clipSoftnessCurve);
//        }
//
//        if (j.contains("Ghost")) {
//            const auto& g = j["Ghost"];
//            meshNode->ghostEnabled = g.value("Enabled", false);
//            meshNode->ghostCount = g.value("Count", 3);
//            meshNode->ghostTimeDelay = g.value("Delay", 0.05f);
//            meshNode->ghostAlphaDecay = g.value("AlphaDecay", 0.5f);
//
//            if (g.contains("Offset")) {
//                auto v = g["Offset"];
//                meshNode->ghostPosOffset = { v[0], v[1], v[2] };
//            }
//        }
//
//        meshNode->RefreshPixelShader();
//        node = meshNode;
//    }
//    // --- Particle Emitter ---
//    else if (type == "ParticleEmitter")
//    {
//        auto particleNode = std::make_shared<ParticleEmitter>();
//
//        // ParticleSetting.h の adl_serializer を利用
//        if (j.contains("Settings")) {
//            try { j.at("Settings").get_to(particleNode->settings); }
//            catch (...) {}
//        }
//        // ParticleRendererSettings (このファイル内のadl_serializerを利用)
//        if (j.contains("RenderSettings")) {
//            try { j.at("RenderSettings").get_to(particleNode->renderSettings); }
//            catch (...) {}
//        }
//        // テクスチャ
//        if (j.contains("Texture")) {
//            std::string p = j["Texture"];
//            if (!p.empty()) particleNode->LoadTexture(p);
//        }
//        node = particleNode;
//    }
//    // --- Empty Node ---
//    else
//    {
//        node = std::make_shared<EffectNode>();
//    }
//
//    // 共通パラメータ
//    node->name = j.value("Name", "Node");
//
//    if (j.contains("Transform")) {
//        const auto& jt = j["Transform"];
//        auto get_vec3 = [&](const char* k, DirectX::XMFLOAT3& v) {
//            if (jt.contains(k)) { auto a = jt[k]; v = { a[0], a[1], a[2] }; }
//            };
//        get_vec3("Position", node->localTransform.position);
//        get_vec3("Rotation", node->localTransform.rotation);
//        get_vec3("Scale", node->localTransform.scale);
//    }
//
//    // Transform カーブ
//    if (j.contains("TransformCurves")) {
//        const auto& jtc = j["TransformCurves"];
//        if (jtc.contains("PosX")) ParseCurve(jtc["PosX"], node->positionCurves[0]);
//        if (jtc.contains("PosY")) ParseCurve(jtc["PosY"], node->positionCurves[1]);
//        if (jtc.contains("PosZ")) ParseCurve(jtc["PosZ"], node->positionCurves[2]);
//        if (jtc.contains("RotX")) ParseCurve(jtc["RotX"], node->rotationCurves[0]);
//        if (jtc.contains("RotY")) ParseCurve(jtc["RotY"], node->rotationCurves[1]);
//        if (jtc.contains("RotZ")) ParseCurve(jtc["RotZ"], node->rotationCurves[2]);
//        if (jtc.contains("ScaleX")) ParseCurve(jtc["ScaleX"], node->scaleCurves[0]);
//        if (jtc.contains("ScaleY")) ParseCurve(jtc["ScaleY"], node->scaleCurves[1]);
//        if (jtc.contains("ScaleZ")) ParseCurve(jtc["ScaleZ"], node->scaleCurves[2]);
//    }
//
//    // 子ノード
//    if (j.contains("Children") && j["Children"].is_array()) {
//        for (const auto& childJson : j["Children"]) {
//            auto childNode = ParseNode(childJson);
//            if (childNode) node->AddChild(childNode);
//        }
//    }
//
//    return node;
//}
//
//bool EffectLoader::SaveEffect(
//    const std::string& filePath,
//    const std::shared_ptr<EffectNode>& rootNode,
//    float life,
//    float fadeIn,
//    float fadeOut,
//    bool loop)
//{
//    if (!rootNode) return false;
//
//    // ★修正: 全体を包むJSONオブジェクトを作成
//    json doc;
//
//    // 設定値を保存
//    doc["Settings"] = {
//        {"LifeTime", life},
//        {"FadeIn", fadeIn},
//        {"FadeOut", fadeOut},
//        {"Loop", loop}
//    };
//
//    // ノードツリーを "Root" キー以下に保存
//    doc["Root"] = SerializeNode(rootNode);
//
//    std::ofstream o(filePath);
//    if (!o.is_open()) return false;
//
//    o << doc.dump(4);
//    return true;
//}
//
//
//// =================================================================
//// SerializeNode: ノードの再帰書き出し
//// =================================================================
//json EffectLoader::SerializeNode(const std::shared_ptr<EffectNode>& node)
//{
//    if (!node) return nullptr;
//
//    json j;
//    j["Name"] = node->name;
//
//    // Transform
//    j["Transform"]["Position"] = { node->localTransform.position.x, node->localTransform.position.y, node->localTransform.position.z };
//    j["Transform"]["Rotation"] = { node->localTransform.rotation.x, node->localTransform.rotation.y, node->localTransform.rotation.z };
//    j["Transform"]["Scale"] = { node->localTransform.scale.x,    node->localTransform.scale.y,    node->localTransform.scale.z };
//
//    // Transform Curves (有効なものだけ)
//    auto& pc = node->positionCurves;
//    auto& rc = node->rotationCurves;
//    auto& sc = node->scaleCurves;
//
//    if (pc[0].IsValid()) j["TransformCurves"]["PosX"] = SerializeCurve(pc[0]);
//    if (pc[1].IsValid()) j["TransformCurves"]["PosY"] = SerializeCurve(pc[1]);
//    if (pc[2].IsValid()) j["TransformCurves"]["PosZ"] = SerializeCurve(pc[2]);
//    if (rc[0].IsValid()) j["TransformCurves"]["RotX"] = SerializeCurve(rc[0]);
//    if (rc[1].IsValid()) j["TransformCurves"]["RotY"] = SerializeCurve(rc[1]);
//    if (rc[2].IsValid()) j["TransformCurves"]["RotZ"] = SerializeCurve(rc[2]);
//    if (sc[0].IsValid()) j["TransformCurves"]["ScaleX"] = SerializeCurve(sc[0]);
//    if (sc[1].IsValid()) j["TransformCurves"]["ScaleY"] = SerializeCurve(sc[1]);
//    if (sc[2].IsValid()) j["TransformCurves"]["ScaleZ"] = SerializeCurve(sc[2]);
//
//    // --- Mesh Emitter ---
//    if (auto meshNode = std::dynamic_pointer_cast<MeshEmitter>(node))
//    {
//        j["Type"] = "MeshEmitter";
//
//        // ※ MeshEmitter.h に GetModelPath() が必要です。
//        // もし無い場合は meshNode->modelPath のようにアクセスできる必要があります。
//        j["Model"] = meshNode->GetModelPath();
//
//        if (meshNode->material) {
//            j["Material"] = SerializeMaterial(meshNode->material);
//        }
//
//        // 専用カーブ
//        if (meshNode->colorCurves[0].IsValid()) j["Curves"]["ColorR"] = SerializeCurve(meshNode->colorCurves[0]);
//        if (meshNode->colorCurves[1].IsValid()) j["Curves"]["ColorG"] = SerializeCurve(meshNode->colorCurves[1]);
//        if (meshNode->colorCurves[2].IsValid()) j["Curves"]["ColorB"] = SerializeCurve(meshNode->colorCurves[2]);
//        if (meshNode->visibilityCurve.IsValid())         j["Curves"]["Visibility"] = SerializeCurve(meshNode->visibilityCurve);
//        if (meshNode->emissiveCurve.IsValid())           j["Curves"]["Emissive"] = SerializeCurve(meshNode->emissiveCurve);
//        if (meshNode->mainUvScrollCurves[0].IsValid())   j["Curves"]["MainUvScrollX"] = SerializeCurve(meshNode->mainUvScrollCurves[0]);
//        if (meshNode->mainUvScrollCurves[1].IsValid())   j["Curves"]["MainUvScrollY"] = SerializeCurve(meshNode->mainUvScrollCurves[1]);
//        if (meshNode->wpoStrengthCurve.IsValid())        j["Curves"]["WpoStrength"] = SerializeCurve(meshNode->wpoStrengthCurve);
//        if (meshNode->fresnelPowerCurve.IsValid())       j["Curves"]["FresnelPower"] = SerializeCurve(meshNode->fresnelPowerCurve);
//        if (meshNode->distortionStrengthCurve.IsValid()) j["Curves"]["DistortionStrength"] = SerializeCurve(meshNode->distortionStrengthCurve);
//        if (meshNode->distUvScrollCurves[0].IsValid())   j["Curves"]["DistUvScrollX"] = SerializeCurve(meshNode->distUvScrollCurves[0]);
//        if (meshNode->distUvScrollCurves[1].IsValid())   j["Curves"]["DistUvScrollY"] = SerializeCurve(meshNode->distUvScrollCurves[1]);
//        if (meshNode->flowStrengthCurve.IsValid())       j["Curves"]["FlowStrength"] = SerializeCurve(meshNode->flowStrengthCurve);
//        if (meshNode->flowSpeedCurve.IsValid())          j["Curves"]["FlowSpeed"] = SerializeCurve(meshNode->flowSpeedCurve);
//        if (meshNode->dissolveCurve.IsValid())           j["Curves"]["Dissolve"] = SerializeCurve(meshNode->dissolveCurve);
//        if (meshNode->clipStartCurve.IsValid())         j["Curves"]["ClipStart"] = SerializeCurve(meshNode->clipStartCurve);
//        if (meshNode->clipEndCurve.IsValid())           j["Curves"]["ClipEnd"] = SerializeCurve(meshNode->clipEndCurve);
//        if (meshNode->clipSoftnessCurve.IsValid()) j["Curves"]["ClipSoftness"] = SerializeCurve(meshNode->clipSoftnessCurve);
//
//        if (meshNode->ghostEnabled) {
//            j["Ghost"]["Enabled"] = meshNode->ghostEnabled;
//            j["Ghost"]["Count"] = meshNode->ghostCount;
//            j["Ghost"]["Delay"] = meshNode->ghostTimeDelay;
//            j["Ghost"]["AlphaDecay"] = meshNode->ghostAlphaDecay;
//            j["Ghost"]["Offset"] = { meshNode->ghostPosOffset.x, meshNode->ghostPosOffset.y, meshNode->ghostPosOffset.z };
//        }
//    }
//
//    // --- Particle Emitter ---
//    else if (auto particleNode = std::dynamic_pointer_cast<ParticleEmitter>(node))
//    {
//        j["Type"] = "ParticleEmitter";
//        j["Settings"] = particleNode->settings;
//        j["RenderSettings"] = particleNode->renderSettings;
//        j["Texture"] = particleNode->texturePath;
//    }
//    // --- Empty Node ---
//    else
//    {
//        j["Type"] = "Empty";
//    }
//
//    // 子ノード
//    if (!node->children.empty()) {
//        j["Children"] = json::array();
//        for (const auto& child : node->children) {
//            j["Children"].push_back(SerializeNode(child));
//        }
//    }
//
//    return j;
//}
#include "EffectLoader.h"
#include "EffectNode.h"
#include "Effect/MeshEmitter.h"
#include "Particle/ParticleEmitter.h"
#include "EffectMaterial.h"
#include "EffectManager.h"
#include "Graphics.h"
#include "JSONManager.h" 
#include <fstream>
#include <iostream>

using json = nlohmann::json;
using namespace DirectX;

namespace nlohmann {
    // EffectMaterialConstants の変換
    template<> struct adl_serializer<EffectMaterialConstants> {
        static void to_json(json& j, const EffectMaterialConstants& c) {
            j = json{
                {"baseColor", {c.baseColor.x, c.baseColor.y, c.baseColor.z, c.baseColor.w}},
                {"emissiveIntensity", c.emissiveIntensity},
                {"mainUvScrollSpeed", {c.mainUvScrollSpeed.x, c.mainUvScrollSpeed.y}},
                {"distortionStrength", c.distortionStrength},
                {"maskEdgeFade", c.maskEdgeFade},
                {"distortionUvScrollSpeed", {c.distortionUvScrollSpeed.x, c.distortionUvScrollSpeed.y}},
                {"dissolveThreshold", c.dissolveThreshold},
                {"dissolveEdgeWidth", c.dissolveEdgeWidth},
                {"maskIntensity", c.maskIntensity},
                {"maskContrast", c.maskContrast},
                {"dissolveEdgeColor", {c.dissolveEdgeColor.x, c.dissolveEdgeColor.y, c.dissolveEdgeColor.z}},
                {"mainTexIndex", c.mainTexIndex},
                {"distortionTexIndex", c.distortionTexIndex},
                {"dissolveTexIndex", c.dissolveTexIndex},
                {"maskTexIndex", c.maskTexIndex},
                {"fresnelColor", {c.fresnelColor.x, c.fresnelColor.y, c.fresnelColor.z}},
                {"fresnelPower", c.fresnelPower},
                {"flipbookWidth", c.flipbookWidth},
                {"flipbookHeight", c.flipbookHeight},
                {"flipbookSpeed", c.flipbookSpeed},
                {"gradientStrength", c.gradientStrength},
                {"gradientTexIndex", c.gradientTexIndex},
                {"wpoStrength", c.wpoStrength},
                {"wpoSpeed", c.wpoSpeed},
                {"wpoFrequency", c.wpoFrequency},
                {"chromaticAberrationStrength", c.chromaticAberrationStrength},
                {"dissolveGlowIntensity", c.dissolveGlowIntensity},
                {"dissolveGlowRange", c.dissolveGlowRange},
                {"uvScrollMode", c.uvScrollMode},
                {"dissolveGlowColor", {c.dissolveGlowColor.x, c.dissolveGlowColor.y, c.dissolveGlowColor.z}},
                {"matCapTexIndex", c.matCapTexIndex},
                {"matCapStrength", c.matCapStrength},
                {"matCapBlend", c.matCapBlend},
                {"clipSoftness", c.clipSoftness},
                {"startEndFadeWidth", c.startEndFadeWidth},
                {"matCapColor", {c.matCapColor.x, c.matCapColor.y, c.matCapColor.z}},
                {"normalTexIndex", c.normalTexIndex},
                {"normalStrength", c.normalStrength},
                {"flowTexIndex", c.flowTexIndex},
                {"flowStrength", c.flowStrength},
                {"flowSpeed", c.flowSpeed},
                {"sideFadeWidth", c.sideFadeWidth},
                {"visibility", c.visibility},
                {"clipStart", c.clipStart},
                {"clipEnd", c.clipEnd},
                {"subTexIndex", c.subTexIndex},
                {"subBlendMode", c.subBlendMode},
                {"subUvScrollSpeed", {c.subUvScrollSpeed.x, c.subUvScrollSpeed.y}},
                {"subTexStrength", c.subTexStrength},
                {"subUvRotationSpeed", c.subUvRotationSpeed},
                {"usePolarCoords", c.usePolarCoords},
                {"toonThreshold", c.toonThreshold},
                {"toonSmoothing", c.toonSmoothing},
                {"toonSteps", c.toonSteps},
                {"toonNoiseStrength", c.toonNoiseStrength},
                {"toonNoiseSpeed", c.toonNoiseSpeed},
                {"toonShadowColor", {c.toonShadowColor.x, c.toonShadowColor.y, c.toonShadowColor.z, c.toonShadowColor.w}},
                {"toonNoiseTexIndex", c.toonNoiseTexIndex},
                {"toonSpecular", {c.toonSpecular.x, c.toonSpecular.y, c.toonSpecular.z, c.toonSpecular.w}},
                {"toonRampTexIndex", c.toonRampTexIndex}
            };
        }
        static void from_json(const json& j, EffectMaterialConstants& c) {
            auto get_val = [&](const char* k, auto& d) { if (j.contains(k)) j.at(k).get_to(d); };
            auto get_vec2 = [&](const char* k, DirectX::XMFLOAT2& d) { if (j.contains(k)) { auto v = j.at(k); d = { v[0], v[1] }; } };
            auto get_vec3 = [&](const char* k, DirectX::XMFLOAT3& d) { if (j.contains(k)) { auto v = j.at(k); d = { v[0], v[1], v[2] }; } };
            auto get_vec4 = [&](const char* k, DirectX::XMFLOAT4& d) { if (j.contains(k)) { auto v = j.at(k); d = { v[0], v[1], v[2], v[3] }; } };

            get_vec4("baseColor", c.baseColor);
            get_val("emissiveIntensity", c.emissiveIntensity);
            get_vec2("mainUvScrollSpeed", c.mainUvScrollSpeed);
            get_val("distortionStrength", c.distortionStrength);
            get_val("maskEdgeFade", c.maskEdgeFade);
            get_vec2("distortionUvScrollSpeed", c.distortionUvScrollSpeed);
            get_val("dissolveThreshold", c.dissolveThreshold);
            get_val("dissolveEdgeWidth", c.dissolveEdgeWidth);
            get_val("maskIntensity", c.maskIntensity);
            get_val("maskContrast", c.maskContrast);
            get_vec3("dissolveEdgeColor", c.dissolveEdgeColor);
            get_val("mainTexIndex", c.mainTexIndex);
            get_val("distortionTexIndex", c.distortionTexIndex);
            get_val("dissolveTexIndex", c.dissolveTexIndex);
            get_val("maskTexIndex", c.maskTexIndex);
            get_vec3("fresnelColor", c.fresnelColor);
            get_val("fresnelPower", c.fresnelPower);
            get_val("flipbookWidth", c.flipbookWidth);
            get_val("flipbookHeight", c.flipbookHeight);
            get_val("flipbookSpeed", c.flipbookSpeed);
            get_val("gradientStrength", c.gradientStrength);
            get_val("gradientTexIndex", c.gradientTexIndex);
            get_val("wpoStrength", c.wpoStrength);
            get_val("wpoSpeed", c.wpoSpeed);
            get_val("wpoFrequency", c.wpoFrequency);
            get_val("chromaticAberrationStrength", c.chromaticAberrationStrength);
            get_val("dissolveGlowIntensity", c.dissolveGlowIntensity);
            get_val("dissolveGlowRange", c.dissolveGlowRange);
            get_val("uvScrollMode", c.uvScrollMode);
            get_vec3("dissolveGlowColor", c.dissolveGlowColor);
            get_val("matCapTexIndex", c.matCapTexIndex);
            get_val("matCapStrength", c.matCapStrength);
            get_val("matCapBlend", c.matCapBlend);
            get_val("clipSoftness", c.clipSoftness);
            get_val("startEndFadeWidth", c.startEndFadeWidth);
            get_vec3("matCapColor", c.matCapColor);
            get_val("normalTexIndex", c.normalTexIndex);
            get_val("normalStrength", c.normalStrength);
            get_val("flowTexIndex", c.flowTexIndex);
            get_val("flowStrength", c.flowStrength);
            get_val("flowSpeed", c.flowSpeed);
            get_val("sideFadeWidth", c.sideFadeWidth);
            get_val("visibility", c.visibility);
            get_val("clipStart", c.clipStart);
            get_val("clipEnd", c.clipEnd);
            get_val("subTexIndex", c.subTexIndex);
            get_val("subBlendMode", c.subBlendMode);
            get_vec2("subUvScrollSpeed", c.subUvScrollSpeed);
            get_val("subTexStrength", c.subTexStrength);
            get_val("subUvRotationSpeed", c.subUvRotationSpeed);
            get_val("usePolarCoords", c.usePolarCoords);
            get_val("toonThreshold", c.toonThreshold);
            get_val("toonSmoothing", c.toonSmoothing);
            get_val("toonSteps", c.toonSteps);
            get_val("toonNoiseStrength", c.toonNoiseStrength);
            get_val("toonNoiseSpeed", c.toonNoiseSpeed);
            get_vec4("toonShadowColor", c.toonShadowColor);
            get_val("toonNoiseTexIndex", c.toonNoiseTexIndex);
            get_vec4("toonSpecular", c.toonSpecular);
            get_val("toonRampTexIndex", c.toonRampTexIndex);
        }
    };
}


// カーブのロード
void ParseCurve(const json& j, EffectCurve& curve)
{
    curve.keys.clear();
    if (j.is_array())
    {
        for (const auto& keyJson : j)
        {
            float t = keyJson.value("time", 0.0f);
            float v = keyJson.value("value", 0.0f);
            curve.AddKey(t, v);
        }
    }
}

// カーブのセーブ
json SerializeCurve(const EffectCurve& curve)
{
    json jArr = json::array();
    for (const auto& k : curve.keys)
    {
        jArr.push_back({ {"time", k.time}, {"value", k.value} });
    }
    return jArr;
}

// マテリアルのロード
void ParseMaterial(const json& jMat, MeshEmitter* meshNode)
{
    if (!meshNode) return;

    // マテリアルがなければ作成
    if (!meshNode->material) meshNode->material = std::make_shared<EffectMaterial>();
    auto mat = meshNode->material;

    // 1. 定数バッファ
    if (jMat.contains("Constants")) {
        jMat.at("Constants").get_to(mat->GetConstants());
    }

    // 2. ブレンドモード
    if (jMat.contains("BlendMode")) {
        mat->SetBlendMode((EffectBlendMode)jMat.value("BlendMode", 0));
    }

    // 3. テクスチャ
    if (jMat.contains("Textures") && jMat["Textures"].is_array()) {
        for (const auto& texJson : jMat["Textures"]) {
            int slot = texJson.value("Slot", -1);
            std::string path = texJson.value("Path", "");
            if (slot >= 0 && !path.empty()) {
                path = JSONManager::ToRelativePath(path);

                auto texture = EffectManager::Get().GetTexture(path);
                mat->SetTexture(slot, path, texture);
            }
        }
    }
}

// マテリアルのセーブ (修正: パス変換適用)
json SerializeMaterial(const std::shared_ptr<EffectMaterial>& mat)
{
    json jMat;
    if (!mat) return jMat;

    jMat["Constants"] = mat->GetConstants();
    jMat["BlendMode"] = (int)mat->GetBlendMode();

    json jTexs = json::array();
    for (int i = 0; i < EffectMaterial::TEXTURE_SLOT_COUNT; ++i) {
        std::string path = mat->GetTexturePath(i);
        if (!path.empty()) {
            // ★修正: テクスチャパスを相対パスへ変換
            jTexs.push_back({ {"Slot", i}, {"Path", JSONManager::ToRelativePath(path)} });
        }
    }
    jMat["Textures"] = jTexs;
    return jMat;
}


std::shared_ptr<EffectNode> EffectLoader::LoadEffect(
    const std::string& filePath,
    float* outLife,
    float* outFadeIn,
    float* outFadeOut,
    bool* outLoop)
{
    std::ifstream file(filePath);
    if (!file.is_open()) return nullptr;

    json j;
    try {
        file >> j;
    }
    catch (const std::exception& e) {
        OutputDebugStringA(("JSON Parse Error: " + std::string(e.what()) + "\n").c_str());
        return nullptr;
    }

    // Settingsブロック
    if (j.contains("Settings")) {
        auto& s = j["Settings"];
        if (outLife)    *outLife = s.value("LifeTime", 2.0f);
        if (outFadeIn)  *outFadeIn = s.value("FadeIn", 0.0f);
        if (outFadeOut) *outFadeOut = s.value("FadeOut", 0.0f);
        if (outLoop)    *outLoop = s.value("Loop", false);
    }

    // Rootノードの読み込み
    if (j.contains("Root")) {
        return ParseNode(j["Root"]);
    }
    // 旧フォーマット対応
    else {
        return ParseNode(j);
    }
}

std::shared_ptr<EffectNode> EffectLoader::ParseNode(const json& j)
{
    std::shared_ptr<EffectNode> node = nullptr;
    std::string type = j.value("Type", "Empty");

    // --- Mesh Emitter ---
    if (type == "MeshEmitter")
    {
        auto meshNode = std::make_shared<MeshEmitter>();
        std::string modelPath = j.value("Model", "");
        if (!modelPath.empty()) {

            meshNode->LoadModel(JSONManager::ToRelativePath(modelPath));
        }

        if (j.contains("Material")) {
            ParseMaterial(j["Material"], meshNode.get());
        }

        // カーブ読み込み
        if (j.contains("Curves")) {
            const auto& jc = j["Curves"];
            if (jc.contains("ColorR")) ParseCurve(jc["ColorR"], meshNode->colorCurves[0]);
            if (jc.contains("ColorG")) ParseCurve(jc["ColorG"], meshNode->colorCurves[1]);
            if (jc.contains("ColorB")) ParseCurve(jc["ColorB"], meshNode->colorCurves[2]);
            if (jc.contains("Visibility")) ParseCurve(jc["Visibility"], meshNode->visibilityCurve);
            if (jc.contains("Emissive"))   ParseCurve(jc["Emissive"], meshNode->emissiveCurve);
            if (jc.contains("MainUvScrollX")) ParseCurve(jc["MainUvScrollX"], meshNode->mainUvScrollCurves[0]);
            if (jc.contains("MainUvScrollY")) ParseCurve(jc["MainUvScrollY"], meshNode->mainUvScrollCurves[1]);
            if (jc.contains("WpoStrength")) ParseCurve(jc["WpoStrength"], meshNode->wpoStrengthCurve);
            if (jc.contains("FresnelPower")) ParseCurve(jc["FresnelPower"], meshNode->fresnelPowerCurve);
            if (jc.contains("DistortionStrength")) ParseCurve(jc["DistortionStrength"], meshNode->distortionStrengthCurve);
            if (jc.contains("DistUvScrollX"))      ParseCurve(jc["DistUvScrollX"], meshNode->distUvScrollCurves[0]);
            if (jc.contains("DistUvScrollY"))      ParseCurve(jc["DistUvScrollY"], meshNode->distUvScrollCurves[1]);
            if (jc.contains("FlowStrength")) ParseCurve(jc["FlowStrength"], meshNode->flowStrengthCurve);
            if (jc.contains("FlowSpeed"))    ParseCurve(jc["FlowSpeed"], meshNode->flowSpeedCurve);
            if (jc.contains("Dissolve")) ParseCurve(jc["Dissolve"], meshNode->dissolveCurve);
            if (jc.contains("ClipStart")) ParseCurve(jc["ClipStart"], meshNode->clipStartCurve);
            if (jc.contains("ClipEnd"))   ParseCurve(jc["ClipEnd"], meshNode->clipEndCurve);
            if (jc.contains("ClipSoftness")) ParseCurve(jc["ClipSoftness"], meshNode->clipSoftnessCurve);
        }

        if (j.contains("Ghost")) {
            const auto& g = j["Ghost"];
            meshNode->ghostEnabled = g.value("Enabled", false);
            meshNode->ghostCount = g.value("Count", 3);
            meshNode->ghostTimeDelay = g.value("Delay", 0.05f);
            meshNode->ghostAlphaDecay = g.value("AlphaDecay", 0.5f);

            if (g.contains("Offset")) {
                auto v = g["Offset"];
                meshNode->ghostPosOffset = { v[0], v[1], v[2] };
            }
        }

        meshNode->RefreshPixelShader();
        node = meshNode;
    }
    // --- Particle Emitter ---
    else if (type == "ParticleEmitter")
    {
        auto particleNode = std::make_shared<ParticleEmitter>();

        if (j.contains("Settings")) {
            try { j.at("Settings").get_to(particleNode->settings); }
            catch (...) {}
        }
        if (j.contains("RenderSettings")) {
            try { j.at("RenderSettings").get_to(particleNode->renderSettings); }
            catch (...) {}
        }
        if (j.contains("Texture")) {
            std::string p = j["Texture"];
            if (!p.empty()) {
                particleNode->LoadTexture(JSONManager::ToRelativePath(p));
            }

        }
        node = particleNode;
    }
    // --- Empty Node ---
    else
    {
        node = std::make_shared<EffectNode>();
    }

    // 共通パラメータ
    node->name = j.value("Name", "Node");

    if (j.contains("Transform")) {
        const auto& jt = j["Transform"];
        auto get_vec3 = [&](const char* k, DirectX::XMFLOAT3& v) {
            if (jt.contains(k)) { auto a = jt[k]; v = { a[0], a[1], a[2] }; }
            };
        get_vec3("Position", node->localTransform.position);
        get_vec3("Rotation", node->localTransform.rotation);
        get_vec3("Scale", node->localTransform.scale);
    }

    if (j.contains("TransformCurves")) {
        const auto& jtc = j["TransformCurves"];
        if (jtc.contains("PosX")) ParseCurve(jtc["PosX"], node->positionCurves[0]);
        if (jtc.contains("PosY")) ParseCurve(jtc["PosY"], node->positionCurves[1]);
        if (jtc.contains("PosZ")) ParseCurve(jtc["PosZ"], node->positionCurves[2]);
        if (jtc.contains("RotX")) ParseCurve(jtc["RotX"], node->rotationCurves[0]);
        if (jtc.contains("RotY")) ParseCurve(jtc["RotY"], node->rotationCurves[1]);
        if (jtc.contains("RotZ")) ParseCurve(jtc["RotZ"], node->rotationCurves[2]);
        if (jtc.contains("ScaleX")) ParseCurve(jtc["ScaleX"], node->scaleCurves[0]);
        if (jtc.contains("ScaleY")) ParseCurve(jtc["ScaleY"], node->scaleCurves[1]);
        if (jtc.contains("ScaleZ")) ParseCurve(jtc["ScaleZ"], node->scaleCurves[2]);
    }

    // 子ノード
    if (j.contains("Children") && j["Children"].is_array()) {
        for (const auto& childJson : j["Children"]) {
            auto childNode = ParseNode(childJson);
            if (childNode) node->AddChild(childNode);
        }
    }

    return node;
}

bool EffectLoader::SaveEffect(
    const std::string& filePath,
    const std::shared_ptr<EffectNode>& rootNode,
    float life,
    float fadeIn,
    float fadeOut,
    bool loop)
{
    if (!rootNode) return false;

    json doc;

    // 設定値を保存
    doc["Settings"] = {
        {"LifeTime", life},
        {"FadeIn", fadeIn},
        {"FadeOut", fadeOut},
        {"Loop", loop}
    };

    // ノードツリー
    doc["Root"] = SerializeNode(rootNode);

    std::ofstream o(filePath);
    if (!o.is_open()) return false;

    o << doc.dump(4);
    return true;
}

json EffectLoader::SerializeNode(const std::shared_ptr<EffectNode>& node)
{
    if (!node) return nullptr;

    json j;
    j["Name"] = node->name;

    // Transform
    j["Transform"]["Position"] = { node->localTransform.position.x, node->localTransform.position.y, node->localTransform.position.z };
    j["Transform"]["Rotation"] = { node->localTransform.rotation.x, node->localTransform.rotation.y, node->localTransform.rotation.z };
    j["Transform"]["Scale"] = { node->localTransform.scale.x,    node->localTransform.scale.y,    node->localTransform.scale.z };

    // Transform Curves
    auto& pc = node->positionCurves;
    auto& rc = node->rotationCurves;
    auto& sc = node->scaleCurves;

    if (pc[0].IsValid()) j["TransformCurves"]["PosX"] = SerializeCurve(pc[0]);
    if (pc[1].IsValid()) j["TransformCurves"]["PosY"] = SerializeCurve(pc[1]);
    if (pc[2].IsValid()) j["TransformCurves"]["PosZ"] = SerializeCurve(pc[2]);
    if (rc[0].IsValid()) j["TransformCurves"]["RotX"] = SerializeCurve(rc[0]);
    if (rc[1].IsValid()) j["TransformCurves"]["RotY"] = SerializeCurve(rc[1]);
    if (rc[2].IsValid()) j["TransformCurves"]["RotZ"] = SerializeCurve(rc[2]);
    if (sc[0].IsValid()) j["TransformCurves"]["ScaleX"] = SerializeCurve(sc[0]);
    if (sc[1].IsValid()) j["TransformCurves"]["ScaleY"] = SerializeCurve(sc[1]);
    if (sc[2].IsValid()) j["TransformCurves"]["ScaleZ"] = SerializeCurve(sc[2]);

    // --- Mesh Emitter ---
    if (auto meshNode = std::dynamic_pointer_cast<MeshEmitter>(node))
    {
        j["Type"] = "MeshEmitter";

        // ★修正: モデルパスを相対パスへ変換
        j["Model"] = JSONManager::ToRelativePath(meshNode->GetModelPath());

        if (meshNode->material) {
            j["Material"] = SerializeMaterial(meshNode->material);
        }

        // 専用カーブ
        if (meshNode->colorCurves[0].IsValid()) j["Curves"]["ColorR"] = SerializeCurve(meshNode->colorCurves[0]);
        if (meshNode->colorCurves[1].IsValid()) j["Curves"]["ColorG"] = SerializeCurve(meshNode->colorCurves[1]);
        if (meshNode->colorCurves[2].IsValid()) j["Curves"]["ColorB"] = SerializeCurve(meshNode->colorCurves[2]);
        if (meshNode->visibilityCurve.IsValid())         j["Curves"]["Visibility"] = SerializeCurve(meshNode->visibilityCurve);
        if (meshNode->emissiveCurve.IsValid())           j["Curves"]["Emissive"] = SerializeCurve(meshNode->emissiveCurve);
        if (meshNode->mainUvScrollCurves[0].IsValid())   j["Curves"]["MainUvScrollX"] = SerializeCurve(meshNode->mainUvScrollCurves[0]);
        if (meshNode->mainUvScrollCurves[1].IsValid())   j["Curves"]["MainUvScrollY"] = SerializeCurve(meshNode->mainUvScrollCurves[1]);
        if (meshNode->wpoStrengthCurve.IsValid())        j["Curves"]["WpoStrength"] = SerializeCurve(meshNode->wpoStrengthCurve);
        if (meshNode->fresnelPowerCurve.IsValid())       j["Curves"]["FresnelPower"] = SerializeCurve(meshNode->fresnelPowerCurve);
        if (meshNode->distortionStrengthCurve.IsValid()) j["Curves"]["DistortionStrength"] = SerializeCurve(meshNode->distortionStrengthCurve);
        if (meshNode->distUvScrollCurves[0].IsValid())   j["Curves"]["DistUvScrollX"] = SerializeCurve(meshNode->distUvScrollCurves[0]);
        if (meshNode->distUvScrollCurves[1].IsValid())   j["Curves"]["DistUvScrollY"] = SerializeCurve(meshNode->distUvScrollCurves[1]);
        if (meshNode->flowStrengthCurve.IsValid())       j["Curves"]["FlowStrength"] = SerializeCurve(meshNode->flowStrengthCurve);
        if (meshNode->flowSpeedCurve.IsValid())          j["Curves"]["FlowSpeed"] = SerializeCurve(meshNode->flowSpeedCurve);
        if (meshNode->dissolveCurve.IsValid())           j["Curves"]["Dissolve"] = SerializeCurve(meshNode->dissolveCurve);
        if (meshNode->clipStartCurve.IsValid())          j["Curves"]["ClipStart"] = SerializeCurve(meshNode->clipStartCurve);
        if (meshNode->clipEndCurve.IsValid())            j["Curves"]["ClipEnd"] = SerializeCurve(meshNode->clipEndCurve);
        if (meshNode->clipSoftnessCurve.IsValid()) j["Curves"]["ClipSoftness"] = SerializeCurve(meshNode->clipSoftnessCurve);

        if (meshNode->ghostEnabled) {
            j["Ghost"]["Enabled"] = meshNode->ghostEnabled;
            j["Ghost"]["Count"] = meshNode->ghostCount;
            j["Ghost"]["Delay"] = meshNode->ghostTimeDelay;
            j["Ghost"]["AlphaDecay"] = meshNode->ghostAlphaDecay;
            j["Ghost"]["Offset"] = { meshNode->ghostPosOffset.x, meshNode->ghostPosOffset.y, meshNode->ghostPosOffset.z };
        }
    }

    // --- Particle Emitter ---
    else if (auto particleNode = std::dynamic_pointer_cast<ParticleEmitter>(node))
    {
        j["Type"] = "ParticleEmitter";
        j["Settings"] = particleNode->settings;
        j["RenderSettings"] = particleNode->renderSettings;

        // ★修正: パーティクルテクスチャパスを相対パスへ変換
        j["Texture"] = JSONManager::ToRelativePath(particleNode->texturePath);
    }
    // --- Empty Node ---
    else
    {
        j["Type"] = "Empty";
    }

    // 子ノード
    if (!node->children.empty()) {
        j["Children"] = json::array();
        for (const auto& child : node->children) {
            j["Children"].push_back(SerializeNode(child));
        }
    }

    return j;
}