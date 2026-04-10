#pragma once
#include "Entity.h"

enum class EnemyType {
    STRAIGHT,
    ZAG,
    DIAGONAL,
    MINELAYER,
    MINE,
    CURVE
};

class Enemy : public Entity {
public:
    Enemy(float x, float y, EnemyType type, int stage, int curveType = 0, float progressDelay = 0.0f);
    virtual void Update(float deltaTime) override;
    virtual void Draw(class D3D11Renderer* renderer) override;
    virtual EntityLayer GetLayer() const override { return EntityLayer::ENEMY; }

    EnemyType GetType() const { return m_type; }
    void TakeDamage(int damage) { m_hp -= damage; if (m_hp <= 0) m_active = false; }
    int GetHP() const { return m_hp; }

    bool ShouldFire();
    bool ShouldDropMine();

    void SetBurstvx(float vx) { m_burstvx = vx; }
    void SetBurstvy(float vy) { m_burstvy = vy; }
    float GetBurstvx() const { return m_burstvx; }
    float GetBurstvy() const { return m_burstvy; }
    int GetBurstRemaining() const { return m_burstRemaining; }
    void SetBurstRemaining(int n) { m_burstRemaining = n; }
    int GetPhase() const { return m_phase; }
    void SetIsLeaving(bool isLeaving) { m_isLeaving = isLeaving; }
    bool HasDroppedMine() const { return m_mineDropped; }
    void SetMineDropped(bool dropped) { m_mineDropped = dropped; }
    float GetSpiralAngle() const { return m_spiralAngle; }
    void SetSpiralAngle(float angle) { m_spiralAngle = angle; }
    int GetMineDropCount() const { return m_mineDropCount; }
    void SetMineDropCount(int n) { m_mineDropCount = n; }
    void SetIsBaby(bool state) { m_isBaby = state; }

private:
    EnemyType m_type;
    int m_hp;
    int m_stage;
    float m_speed;
    float m_fireTimer;
    float m_fireCooldown;
    
    int m_dir;
    float m_moveTimer;
    float m_triggerY;
    float m_spawnX;      // Fixed original X for rotation center
    
    float m_curveProgress;
    float m_progressDelay; 
    int m_curveType; 
    
    float m_mineTimer;
    float m_mineCooldown;
    bool m_mineDropped; // Only 1 mine per ship fallback
    int m_mineDropCount; // Strategic count for 2-mine limit

    float m_stayTimer;
    float m_maxStayTime;
    bool m_isLeaving;
    bool m_isBaby;

    float m_burstTimer; 
    int m_burstRemaining;
    float m_burstvx, m_burstvy;
    float m_loopAngle;
    float m_spiralAngle; // For MINE spiral patterns
    int m_phase; // 0: Entry, 1: Loop, 2: Retreat

    void MoveStraight(float deltaTime);
    void MoveZag(float deltaTime);
    void MoveDiagonal(float deltaTime);
    void MoveMineLayer(float deltaTime);
    void MoveCurve(float deltaTime);
};
