#pragma once
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include <d3d11.h>

class Enemy;
class ModelRenderer;
struct RenderContext;

class EnemyManager
{
private:
    EnemyManager() = default;
    ~EnemyManager() = default;

public:
    static EnemyManager& Instance()
    {
        static EnemyManager instance;
        return instance;
    }

    void Update(float dt);
    void Render(ModelRenderer* renderer);

    std::shared_ptr<Enemy> CreateEnemyTest(ID3D11Device* device, const DirectX::XMFLOAT3& position);

    void Remove(std::shared_ptr<Enemy> enemy);

    void Clear();

    void OnGUI();

    std::shared_ptr<Enemy> GetNearestEnemy(const DirectX::XMFLOAT3& targetPos, float range);


    void RegisterEnemy(std::shared_ptr<Enemy> enemy);
private:
    std::vector<std::shared_ptr<Enemy>> enemies;
    std::vector<std::shared_ptr<Enemy>> removeQueue;
};
