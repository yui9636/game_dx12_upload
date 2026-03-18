#pragma once
#include "Character/Character.h"

// ‘O•ûéŒ¾
class Actor;

class Enemy : public Character
{
public:
    Enemy();
    virtual ~Enemy() override = default;

    void Initialize(ID3D11Device* device) override;

    virtual void Update(float dt) override;
    virtual void Render(ModelRenderer* renderer) override;

protected:
    virtual void OnDamaged() override;
    virtual void OnDead() override;

protected:
    bool isDead = false;
};