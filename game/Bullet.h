#pragma once
#include "Entity.h"
#include <vector>
#include <memory>
#include <d3d11.h>

enum class BulletType {
    PLAYER_MAIN,
    PLAYER_VULCAN,
    PLAYER_PLASMA,
    ENEMY,
    BOSS
};
class Enemy;
class Boss;

class Bullet : public Entity {
public:
    Bullet();
    virtual void Update(float deltaTime) override;
    virtual void Draw(class D3D11Renderer* renderer) override;
    virtual EntityLayer GetLayer() const override { return EntityLayer::BULLET; }

    void Fire(float x, float y, float vx, float vy, bool isFriendly, BulletType type, int variant = 0, bool isHoming = false);
    void SetVelocity(float vx, float vy) { m_vx = vx; m_vy = vy; }
    void UpdateHoming(float deltaTime, const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<std::unique_ptr<Boss>>& bosses);
    void SetImage(ID3D11ShaderResourceView* img) { m_pImg = img; }
    void SetScale(float s) { m_scale = s; }
    void SetConcentrating(bool b) { m_isConcentrating = b; }
    bool IsHoming() const { return m_isHoming; }
    bool IsFriendly() const { return m_friendly; }
    bool TakeDamage(int d) { if (m_isDestructible) { m_hp -= d; if (m_hp <= 0) { m_hp = 0; return true; } } return false; }
    void SetHP(int hp) { m_hp = hp; m_isDestructible = true; }
    bool IsDestructible() const { return m_isDestructible; }
    BulletType GetType() const { return m_type; }
    int GetVariant() const { return m_variant; }
    void SetVariant(int v) { m_variant = v; }

private:
    float m_vx, m_vy;
    float m_baseX, m_baseY;
    float m_speed;
    float m_lifeTime;
    float m_maxLifeTime;
    float m_growthRate;
    float m_spawnDelay;
    float m_rotationAngle;
    float m_rotationRate;
    ID3D11ShaderResourceView* m_pImg; 
    bool m_friendly;
    BulletType m_type;
    int m_variant;
    bool m_isHoming;
    bool m_isTargetLocked;
    bool m_isSmall; 
    float m_scale;

    bool m_isPattern;
    bool m_isConcentrating;
    bool m_isTurret;
    bool m_isAccelPattern;
    int m_hp;
    bool m_isDestructible;
    int m_phase; 
    float m_phaseTimer;
    
    bool m_isOrbiting;
    int m_orbitBossId;
    float m_orbitAngle;
    float m_orbitRadius;
    float m_orbitSpeed;
    float m_orbitReleaseTimer;
    Vector2 m_orbitAnchorOffset;
    
    bool m_noDamage;
    bool m_isHeartbeat;
    Vector2 m_hbCenter;
    float m_hbDist;
    float m_hbAngle;
    float m_hbPhase;
    float m_hbSpeed;
    float m_orbitIndex;

public:
    void UpdatePattern(float deltaTime, Vector2 target);
    void SetPattern(bool isPattern) { m_isPattern = isPattern; m_phase = 0; m_phaseTimer = 0.0f; }
    void SetPhase(int p) { m_phase = p; }
    int GetPhase() const { return m_phase; }
    void SetPhaseTimer(float t) { m_phaseTimer = t; }
    float GetPhaseTimer() const { return m_phaseTimer; }
    void AddPhaseTimer(float dt) { m_phaseTimer += dt; }
    void SetIsSmall(bool isSmall) { m_isSmall = isSmall; }
    bool IsPattern() const { return m_isPattern; }
    bool IsSmall() const { return m_isSmall; }
    void SetAccelPattern(bool accel) { m_isAccelPattern = accel; }
    bool IsAccelPattern() const { return m_isAccelPattern; }

    void SetOrbit(bool orbiting, int bossId, float radius, float speed, float angle, float releaseTimer, Vector2 anchorOffset) {
        m_isOrbiting = orbiting; m_orbitBossId = bossId; m_orbitRadius = radius; m_orbitSpeed = speed; m_orbitAngle = angle;
        m_orbitReleaseTimer = releaseTimer; m_orbitAnchorOffset = anchorOffset; m_phaseTimer = 0.0f;
    }
    bool IsOrbiting() const { return m_isOrbiting; }
    float GetOrbitAngle() const { return m_orbitAngle; }
    void SetOrbitIndex(float i) { m_orbitIndex = i; }
    float GetOrbitIndex() const { return m_orbitIndex; }
    void UpdateOrbit(float deltaTime, Vector2 target, const std::vector<std::unique_ptr<Boss>>& bosses);

    void SetTurret(bool t) { m_isTurret = t; }
    bool IsTurret() const { return m_isTurret; }

    void SetHeartbeat(bool hb, Vector2 center, float dist, float angle, float phaseOffset = 0.0f) {
        m_isHeartbeat = hb; m_hbCenter = center; m_hbDist = dist; m_hbAngle = angle; m_hbPhase = phaseOffset;
        m_isPattern = hb; 
    }
    bool IsHeartbeat() const { return m_isHeartbeat; }

    void SetNoDamage(bool noDamage) { m_noDamage = noDamage; }
    bool IsNoDamage() const { return m_noDamage; }
    
    void SetLifeTime(float lt) { m_maxLifeTime = lt; }
    void SetGrowthRate(float gr) { m_growthRate = gr; }
    void SetSpawnDelay(float d) { m_spawnDelay = d; }
    void SetRotationRate(float r) { m_rotationRate = r; }
    void SetAlpha(float a) { m_alpha = a; }
    float GetAlpha() const { return m_alpha; }
    float GetLifeTime() const { return m_lifeTime; }

private:
    float m_alpha;
};
