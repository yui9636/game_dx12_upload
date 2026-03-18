#pragma once
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include <d3d11.h>

// 前方宣言
class Enemy;
class ModelRenderer;
struct RenderContext;

class EnemyManager
{
private:
    // シングルトン
    EnemyManager() = default;
    ~EnemyManager() = default;

public:
    static EnemyManager& Instance()
    {
        static EnemyManager instance;
        return instance;
    }

    // 更新・描画
    void Update(float dt);
    void Render(ModelRenderer* renderer);

    // 敵の生成関数（EnemyTest専用）
    std::shared_ptr<Enemy> CreateEnemyTest(ID3D11Device* device, const DirectX::XMFLOAT3& position);

    // 敵の削除予約
    void Remove(std::shared_ptr<Enemy> enemy);

    // 全消去
    void Clear();

    void OnGUI();

    // 一番近い敵を取得（ロックオンカメラ用）
    // range: 検索範囲（無限ならFLT_MAX）
    std::shared_ptr<Enemy> GetNearestEnemy(const DirectX::XMFLOAT3& targetPos, float range);


    void RegisterEnemy(std::shared_ptr<Enemy> enemy);
private:
    std::vector<std::shared_ptr<Enemy>> enemies;
    std::vector<std::shared_ptr<Enemy>> removeQueue;
};