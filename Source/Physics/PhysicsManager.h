#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <memory>
#include <Entity\Entity.h>
#include <d3d11.h>
#include <DirectXMath.h>

// Jolt 側で使う物理オブジェクトの大分類レイヤー。
// 静的オブジェクトと動的オブジェクトを分け、BroadPhase の判定を軽くするために使う。
namespace Layers {
    // 地形や壁など、動かない物理オブジェクト用レイヤー。
    static constexpr JPH::ObjectLayer NON_MOVING = 0;

    // キャラクターや弾など、移動する物理オブジェクト用レイヤー。
    static constexpr JPH::ObjectLayer MOVING = 1;

    // 登録している ObjectLayer の総数。
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
};

// 物理レイキャストの結果をエンジン側で扱いやすい形にまとめる構造体。
struct PhysicsRaycastResult {
    // レイが何かに当たったかどうか。
    bool hasHit = false;

    // ヒットした Body に紐づけられている EntityID。
    uint32_t entityID = 0;

    // レイ開始点からヒット位置までの距離。
    float distance = 0.0f;

    // ワールド座標でのヒット位置。
    DirectX::XMFLOAT3 position;

    // ヒット面のワールド法線。
    DirectX::XMFLOAT3 normal;
};

// Jolt Physics の初期化・更新・終了・簡易クエリをまとめて管理するシングルトン。
class PhysicsManager {
public:
    // PhysicsManager の唯一のインスタンスを取得する。
    static PhysicsManager& Instance();

    // Jolt の allocator / factory / physics system / job system を初期化する。
    void Initialize();

    // 物理ワールドを指定時間だけ進める。
    void Update(float deltaTime);

    // Jolt の登録解除と物理関連リソースの解放を行う。
    void Finalize();

    // Jolt の PhysicsSystem 本体を取得する。
    JPH::PhysicsSystem* GetJoltSystem() { return m_physicsSystem.get(); }

    // Body の作成・削除・位置更新に使う BodyInterface を取得する。
    JPH::BodyInterface& GetBodyInterface() { return m_physicsSystem->GetBodyInterface(); }

    // ワールド空間にレイを飛ばし、最初に当たった物理 Body の情報を返す。
    PhysicsRaycastResult CastRay(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance = 1000.0f);

private:
    // シングルトン専用なので外部から生成させない。
    PhysicsManager();

    // シングルトン専用なので外部から破棄させない。
    ~PhysicsManager();

    // Jolt の物理ワールド本体。
    std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;

    // 物理更新中の一時メモリ確保に使う allocator。
    std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;

    // 物理計算を並列実行するための job system。
    std::unique_ptr<JPH::JobSystemThreadPool> m_jobSystem;

    // ObjectLayer から BroadPhaseLayer へ変換する内部実装クラス。
    class BPLayerInterfaceImpl;

    // ObjectLayer と BroadPhaseLayer の衝突可否を判定する内部実装クラス。
    class ObjectVsBroadPhaseLayerFilterImpl;

    // ObjectLayer 同士の衝突可否を判定する内部実装クラス。
    class ObjectLayerPairFilterImpl;

    // ObjectLayer と BroadPhaseLayer の対応表。
    std::unique_ptr<BPLayerInterfaceImpl> m_bpLayerInterface;

    // ObjectLayer と BroadPhaseLayer のフィルタ。
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_objVsBpFilter;

    // ObjectLayer 同士のフィルタ。
    std::unique_ptr<ObjectLayerPairFilterImpl> m_objPairFilter;
};
