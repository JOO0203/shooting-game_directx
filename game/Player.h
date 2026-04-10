#pragma once
#include "Entity.h"

class Player : public Entity {
public:
    Player();
    virtual void Update(float deltaTime) override;
    virtual void Draw(class D3D11Renderer* renderer) override;
    virtual EntityLayer GetLayer() const override { return EntityLayer::PLAYER; }

    void SetBounds(int w, int h) { m_boundW = w; m_boundH = h; }
    void LevelUp() { if (m_powerLevel < 6) m_powerLevel++; }
    int GetPowerLevel() const { return m_powerLevel; }
    void Reset();
    void StartRespawn();
    void SetInvincible(float duration);
    void SetShield(bool active) { m_isShieldActive = active; }
    bool IsRespawning() const { return m_isRespawning; }
    bool IsInvincible() const { return m_invincibleTimer > 0; }
    int GetPlasmaLevel() const { return m_plasmaLevel; }
    int GetVulcanLevel() const { return m_vulcanLevel; }
    void EnablePlasma();
    void EnableVulcan();
    void MaxUpgrade();
    int GetDamage() const { return 1 + (m_powerLevel / 3); }
    void SetHoming(bool enabled) { m_homingEnabled = enabled; }
    bool IsHomingEnabled() const { return m_homingEnabled; }

private:
    float m_speed;
    int m_boundW, m_boundH;
    int m_powerLevel;
    bool m_isRespawning;
    float m_invincibleTimer;
    int m_plasmaLevel;
    int m_vulcanLevel;
    bool m_homingEnabled;
    bool m_isShieldActive;
    int m_state; 
    int m_animFrame;
    float m_animTimer;
    void HandleInput(float deltaTime);
};
