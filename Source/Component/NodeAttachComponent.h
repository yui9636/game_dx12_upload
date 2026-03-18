#pragma once
#include "Component/Component.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <DirectXMath.h>

class Model;
class Actor;

/// @brief 登録された接続点（ソケット）の定義データ
struct NodeSocket
{
    std::string name;           // ソケット名 (例: "Hand_R_Weapon")
    std::string parentBoneName; // 親となるボーン名 (例: "mixamorig:RightHand")

    // オフセット設定（親ボーンからの相対）
    DirectX::XMFLOAT3 offsetPos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetRotDeg = { 0.0f, 0.0f, 0.0f }; // Euler Degrees
    DirectX::XMFLOAT3 offsetScale = { 1.0f, 1.0f, 1.0f };

    // 実行時キャッシュ
    int cachedBoneIndex = -1;
};

class NodeAttachComponent : public Component
{
public:
    /// オフセットの適用空間
    enum class OffsetSpace
    {
        NodeLocal,   // ボーンの回転・スケールに従う（武器装備など）
        ModelLocal   // モデル原点基準だがボーン位置には追従（浮遊オプションなど）
    };

public:
    const char* GetName() const override { return "NodeAttach"; }

    void Start() override;
    void Update(float dt) override;
    void OnGUI() override;

    // ========================================================================
    // ?? 最強機能 1: ソケット管理システム (外部アクセス用)
    // ========================================================================

    /// @brief 新しいソケットを登録・更新する
    void RegisterSocket(const std::string& socketName, const std::string& boneName,
        const DirectX::XMFLOAT3& pos = { 0,0,0 },
        const DirectX::XMFLOAT3& rotDeg = { 0,0,0 },
        const DirectX::XMFLOAT3& scale = { 1,1,1 });

    /// @brief ソケットを削除する
    void UnregisterSocket(const std::string& socketName);

    /// @brief 登録済みソケットのワールド行列を取得する
    /// @return 成功したら true
    bool GetSocketWorldTransform(const std::string& socketName, DirectX::XMFLOAT4X4& outWorld);

    /// @brief 指定したボーンのワールド行列を直接取得する（ソケット登録なし用）
    bool GetBoneWorldTransform(const std::string& boneName, DirectX::XMFLOAT4X4& outWorld);

    // ========================================================================
    // ?? 最強機能 2: 自身のアタッチ制御 (自分をどこかにくっつける)
    // ========================================================================

    /// @brief ターゲット（親となるアクター）を設定する
    void BindTargetActor(std::shared_ptr<Actor> actor);

    /// @brief 自分自身を、登録済みのソケットに追従させる
    void AttachSelfToSocket(const std::string& socketName);

    /// @brief 自分自身を、ボーン名指定で追従させる（簡易版）
    void AttachSelfToBone(const std::string& boneName);

    /// @brief 追従を解除する
    void Detach();

    /// @brief 現在アタッチ中か？
    bool IsAttached() const { return isAttached; }

    // 詳細設定セッター
    void SetOffset(const DirectX::XMFLOAT3& t) { defaultOffsetPos = t; }
    void SetEuler(const DirectX::XMFLOAT3& r) { defaultOffsetRot = r; }
    void SetScale(const DirectX::XMFLOAT3& s) { defaultOffsetScale = s; }
    void SetOffsetSpace(OffsetSpace s) { offsetSpace = s; }

    // 静的ヘルパー
    static DirectX::XMFLOAT3 GetWorldPosition_NodeLocal(
        const ::Model* model, int nodeIndex, const DirectX::XMFLOAT3& offsetLocal);

private:
    // 内部計算ロジック
    DirectX::XMFLOAT4X4 CalcWorldMatrix(int boneIndex, const ::Model* model, const DirectX::XMFLOAT4X4& actorWorld,
        const DirectX::XMFLOAT3& t, const DirectX::XMFLOAT3& r, const DirectX::XMFLOAT3& s) const;

    // ボーン名の解決
    int ResolveBoneIndex(const ::Model* model, const std::string& name, int& cacheIndex);

    // GUI用ヘルパー: ボーン一覧の更新
    void UpdateBoneListCache(const ::Model* model);

private:
    // ターゲット（親）
    std::shared_ptr<Actor> targetActor;
    const ::Model* lastModelPtr = nullptr;

    // ソケットデータ
    std::unordered_map<std::string, NodeSocket> sockets;

    // 自身の追従設定
    bool isAttached = false;
    bool useSocketForSelf = false;
    std::string currentAttachName;

    // デフォルトオフセット（ソケットを使わない場合用）
    DirectX::XMFLOAT3 defaultOffsetPos{ 0,0,0 };
    DirectX::XMFLOAT3 defaultOffsetRot{ 0,0,0 };
    DirectX::XMFLOAT3 defaultOffsetScale{ 1,1,1 };
    OffsetSpace       offsetSpace = OffsetSpace::NodeLocal;

    // GUI / デバッグ用
    bool showDebugSockets = false;
    std::vector<std::string> guiBoneNameCache; // GUIのコンボボックス用
    int guiSelectedBoneIndex = 0;              // GUIでの選択状態
};