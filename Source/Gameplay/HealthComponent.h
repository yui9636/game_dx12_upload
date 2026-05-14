#pragma once
// HealthComponent は health/maxHealth/invincibleTimer/isInvincible を保持し、関連システムが実行時状態として参照する。

struct HealthComponent {
    int health = 100;
    int maxHealth = 100;
    float invincibleTimer = 0.0f;
    bool isInvincible = false;
    bool isDead = false;
    int lastDamage = 0;
};
