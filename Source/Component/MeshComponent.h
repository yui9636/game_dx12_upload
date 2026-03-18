#pragma once
#include <string>
#include <memory>

// Modelクラスの前方宣言 (Model.hをインクルードしないことでビルド時間を短縮)
class Model;

/**
 * @brief 描画リソースと表示フラグを管理するコンポーネント
 */
struct MeshComponent {
    // モデルデータへの共有ポインタ
    std::shared_ptr<Model> model;

    // ファイルパス（シリアライズ/保存用）
    std::string modelFilePath;

    // 各種フラグ
    bool isVisible = true;      // 描画するか
    bool castShadow = true;     // 影を落とすか
    bool isDebugModel = false;  // デバッグ表示用か

 
};