#pragma once
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include "UIDamagePopup.h"

class DamageTextManager
{
private:
    DamageTextManager() = default;
    ~DamageTextManager() = default;

public:
    static DamageTextManager& Instance()
    {
        static DamageTextManager instance;
        return instance;
    }

    void Initialize();
    void Update(float dt);
    void Render(const RenderContext& rc);

    void Spawn(const DirectX::XMFLOAT3& position, int damage);

private:
    static const int POOL_SIZE = 64;
    std::vector<std::shared_ptr<UIDamagePopup>> pool;
};
