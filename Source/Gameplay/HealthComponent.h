#pragma once

struct HealthComponent {
    int health = 100;
    int maxHealth = 100;
    float invincibleTimer = 0.0f;
    bool isInvincible = false;
    bool isDead = false;
    int lastDamage = 0;
};
