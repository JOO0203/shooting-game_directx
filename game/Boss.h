#pragma once
#include "Entity.h"
#include <vector>

enum class BossState {
    INTRO,
    ACTIVE,
    DYING
};

class Boss : public Entity {
public:
    Boss(float x, float y, int stage, int bossId = 0);
    virtual void Update(float deltaTime) override;
    virtual void Draw(class D3D11Renderer* renderer) override;
    virtual EntityLayer GetLayer() const override { return EntityLayer::BOSS; }

    void TakeDamage(int damage, float bulletX = -1.0f);
    BossState GetState() const { return m_state; }
    bool ShouldFire();
    bool IsActuallyDead() const { return m_isActuallyDead; }
    
    int GetCurrentPattern() const { return m_patternSequence.empty() ? 0 : m_patternSequence[m_patternIndex]; }
    float GetPatternTimer() const { return m_patternTimer; }
    int GetHP() const { return m_hp; }
    int GetMaxHP() const { return m_maxHp; }
    int GetBossId() const { return m_bossId; }
    int GetStage() const { return m_stage; }
    float GetBulletSpeedMultiplier() const { return m_bulletSpeedMultiplier; }
    
    // 3-part HP for Stage 3
    int GetMainHP() const { return m_hpMain; }
    int GetLeftHP() const { return m_hpL; }
    int GetRightHP() const { return m_hpR; }

    bool CheckAndResetWingLExplosion() { if (m_hpL <= 0 && !m_wingLExploded) { m_wingLExploded = true; return true; } return false; }
    bool CheckAndResetWingRExplosion() { if (m_hpR <= 0 && !m_wingRExploded) { m_wingRExploded = true; return true; } return false; }

    // Pattern State Management
    float GetPatternAngle1() const { return m_pa1; }
    void AddPatternAngle1(float a) { m_pa1 += a; }
    void SetPatternAngle1(float a) { m_pa1 = a; }
    float GetPatternAngle2() const { return m_pa2; }
    void AddPatternAngle2(float a) { m_pa2 += a; }
    void SetPatternAngle2(float a) { m_pa2 = a; }
    float GetPatternAngle3() const { return m_pa3; }
    void AddPatternAngle3(float a) { m_pa3 += a; }
    void SetPatternAngle3(float a) { m_pa3 = a; }
    float GetPatternAngle4() const { return m_pa4; }
    void AddPatternAngle4(float a) { m_pa4 += a; }
    void SetPatternAngle4(float a) { m_pa4 = a; }
    
    float GetPatternTimer1() const { return m_pt1; }
    void AddPatternTimer1(float dt) { m_pt1 += dt; }
    void SetPatternTimer1(float t) { m_pt1 = t; }
    float GetPatternTimer2() const { return m_pt2; }
    void AddPatternTimer2(float dt) { m_pt2 += dt; }
    void SetPatternTimer2(float t) { m_pt2 = t; }
    float GetPatternTimer3() const { return m_pt3; }
    void AddPatternTimer3(float dt) { m_pt3 += dt; }
    void SetPatternTimer3(float t) { m_pt3 = t; }
    float GetPatternTimer4() const { return m_pt4; }
    void AddPatternTimer4(float dt) { m_pt4 += dt; }
    void SetPatternTimer4(float t) { m_pt4 = t; }
    float GetPatternTimer5() const { return m_pt5; }
    void AddPatternTimer5(float dt) { m_pt5 += dt; }
    void SetPatternTimer5(float t) { m_pt5 = t; }

    bool GetPatternBool1() const { return m_pb1; }
    void SetPatternBool1(bool b) { m_pb1 = b; }
    bool GetPatternBool2() const { return m_pb2; }
    void SetPatternBool2(bool b) { m_pb2 = b; }

private:
    BossState m_state = BossState::INTRO;
    int m_hp = 0, m_maxHp = 0;
    int m_hpMain = 0, m_maxHpMain = 0;
    int m_hpL = 0, m_maxHpL = 0;
    int m_hpR = 0, m_maxHpR = 0;
    bool m_wingLExploded = false;
    bool m_wingRExploded = false;
    bool m_isUltimateFired = false;
    float m_fireTimer = 0.0f;
    float m_patternTimer = 0.0f;
    std::vector<int> m_patternSequence;
    int m_patternIndex = 0;
    int m_stage = 1;
    int m_bossId = 0;
    int m_animFrame = 0;
    float m_animTimer = 0.0f;
    float m_bulletSpeedMultiplier = 1.0f;

    float m_deathTimer = 0.0f;
    bool m_isActuallyDead = false;

    // Independent pattern states to prevent static variable issues
    float m_pa1 = 0.0f; // Pattern Angle 1
    float m_pa2 = 0.0f; // Pattern Angle 2
    float m_pt1 = 0.0f; // Pattern Timer 1
    float m_pt2 = 0.0f; // Pattern Timer 2
    float m_pt3 = 0.0f; // Pattern Timer 3
    float m_pt4 = 0.0f; // Pattern Timer 4
    float m_pt5 = 0.0f; // Pattern Timer 5
    bool m_pb1 = false; // Pattern Bool 1
    bool m_pb2 = false; // Pattern Bool 2

    float m_pa3 = 0.0f;
    float m_pa4 = 0.0f;
};
