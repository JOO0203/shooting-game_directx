#include "Bullet.h"
#include "D3D11Renderer.h"
#include "ResourceManager.h"
#include "Enemy.h"
#include "Boss.h"
#include <cmath>
#include <vector>
#include <memory>

Bullet::Bullet() : m_pImg(nullptr), m_variant(0), m_lifeTime(0.0f), m_maxLifeTime(0.0f), m_growthRate(0.0f), m_spawnDelay(0.0f), m_rotationAngle(0.0f), m_rotationRate(0.0f), m_friendly(false), m_type(BulletType::ENEMY), m_vx(0.0f), m_vy(0.0f), m_baseX(0.0f), m_baseY(0.0f), m_speed(0.0f), m_isHoming(false), m_isTargetLocked(false), m_isSmall(false), m_scale(1.0f), m_isPattern(false), m_isConcentrating(false), m_phase(0), m_phaseTimer(0.0f), m_isHeartbeat(false), m_hbDist(0.0f), m_hbAngle(0.0f), m_hbPhase(0.0f), m_isOrbiting(false), m_isTurret(false), m_isAccelPattern(false), m_hp(0), m_isDestructible(false), m_noDamage(false), m_alpha(1.0f), m_orbitIndex(0.0f) {
    m_active = false;
}

void Bullet::Update(float deltaTime) {
    if (!m_active) return;
    
    if (m_spawnDelay > 0.0f) {
        m_spawnDelay -= deltaTime;
        if (m_spawnDelay > 0.0f) return;
        else deltaTime = -m_spawnDelay; // Apply remaining time to this frame
    }

    if (m_rotationRate != 0.0f) {
        m_rotationAngle += m_rotationRate * deltaTime;
    }
    
    m_lifeTime += deltaTime;
    m_baseX += m_vx * deltaTime;
    m_baseY += m_vy * deltaTime;

    if (m_growthRate > 0.001f || m_growthRate < -0.001f) {
        m_scale += m_growthRate * deltaTime;
        m_radius = 15.0f * m_scale;
    }

    if (m_isConcentrating && m_lifeTime > 0.02f) {
        // Very sharp curve towards center line (vx -> 0)
        float accel = 5500.0f; // Extreme convergence (was 2800)
        if (m_vx < -30.0f) m_vx += accel * deltaTime;
        else if (m_vx > 30.0f) m_vx -= accel * deltaTime;
        else m_vx = 0.0f; 
    }

    if (m_variant >= 5000) {
        if (m_speed > 0.01f) {
            float nx = -m_vy / m_speed;
            float ny = m_vx / m_speed;
            float offset = sinf(m_lifeTime * 8.0f) * 22.0f; 
            m_pos.x = m_baseX + nx * offset;
            m_pos.y = m_baseY + ny * offset;
        } else {
            m_pos.x = m_baseX; m_pos.y = m_baseY;
        }
    } else {
        m_pos.x = m_baseX;
        m_pos.y = m_baseY;
    }

    if (m_pos.y < -150 || m_pos.y > 1174 || m_pos.x < -150 || m_pos.x > 1174) {
        m_active = false;
    }

    if (m_maxLifeTime > 0.01f && m_lifeTime > m_maxLifeTime) {
        m_active = false;
    }
}

void Bullet::Fire(float x, float y, float vx, float vy, bool friendly, BulletType type, int variant, bool isHoming) {
    m_pos.x = m_baseX = x;
    m_pos.y = m_baseY = y;
    m_vx = vx;
    m_vy = vy;
    m_speed = sqrtf(m_vx * m_vx + m_vy * m_vy);
    m_friendly = friendly;
    m_type = type;
    m_variant = variant;
    m_active = true;
    m_lifeTime = 0.0f;
    m_maxLifeTime = 0.0f;
    m_growthRate = 0.0f;
    m_spawnDelay = 0.0f;
    m_rotationAngle = 0.0f;
    m_rotationRate = 0.0f;
    m_pImg = nullptr;
    m_scale = 1.0f;
    m_hp = 0;
    m_isDestructible = false;
    m_isConcentrating = false;
    m_isHoming = isHoming;
    m_isTargetLocked = false;
    m_isSmall = false;
    m_isPattern = false;
    m_isTurret = false;
    m_isAccelPattern = false;
    m_phase = 0;
    m_phaseTimer = 0.0f;

    m_isOrbiting = false;
    m_isHeartbeat = false;
    m_noDamage = false;
    m_orbitBossId = 0;
    m_orbitAngle = 0.0f;
    m_orbitRadius = 0.0f;
    m_orbitSpeed = 0.0f;
    m_orbitReleaseTimer = 0.0f;
    m_alpha = 1.0f;
    
    if (m_type == BulletType::BOSS) m_radius = 6.0f;
    else m_radius = 4.0f;
}

void Bullet::UpdatePattern(float deltaTime, Vector2 target) {
    if (!m_active) return;

    if (m_isHeartbeat) {
        m_hbPhase += deltaTime * 2.8f;
        m_hbCenter.y += deltaTime * 65.0f; // Systematic fall
        float currentDist = m_hbDist + sinf(m_hbPhase) * 60.0f;
        m_pos.x = m_hbCenter.x + cosf(m_hbAngle) * currentDist;
        m_pos.y = m_hbCenter.y + sinf(m_hbAngle) * currentDist;
        m_baseX = m_pos.x; m_baseY = m_pos.y;
        return;
    }

    m_phaseTimer += deltaTime;

    if (m_phase == 0) {
        if (m_phaseTimer > 0.40f) m_phase = 1;
    } else if (m_phase == 1) {
        // Precise approximation to match original powf(0.01f, deltaTime)
        // 1.0 - 4.6 * dt matches the decay curve of 0.01^dt more accurately for 60-120FPS
        float damping = (1.0f - 4.6f * deltaTime); 
        if (damping < 0.1f) damping = 0.1f;
        m_vx *= damping;
        m_vy *= damping;

        if (m_phaseTimer > 1.20f && !m_isTurret) {
            m_phase = 2;
            float dx = target.x - m_pos.x;
            float dy = target.y - m_pos.y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist > 0.1f) {
                float advanceSpeed = 600.0f; 
                m_vx = (dx / dist) * advanceSpeed;
                m_vy = (dy / dist) * advanceSpeed;
            } else {
                m_vx = 0; m_vy = 600.0f;
            }
        }
    } else if (m_phase == 2 && m_isAccelPattern) {
        // Precise approximation to match original powf(4.0f, deltaTime)
        // 1.0 + 1.4 * dt matches the acceleration curve of 4.0^dt
        float accelFactor = (1.0f + 1.4f * deltaTime); 
        m_vx *= accelFactor;
        m_vy *= accelFactor;
        
        float v2 = m_vx*m_vx + m_vy*m_vy;
        if (v2 > 3240000.0f) { // 1800^2
            float speed = sqrtf(v2);
            m_vx = (m_vx / speed) * 1800.0f;
            m_vy = (m_vy / speed) * 1800.0f;
        }
    }
    
    m_pos.x += m_vx * deltaTime;
    m_pos.y += m_vy * deltaTime;

    if (m_pos.y < -150 || m_pos.y > 1174 || m_pos.x < -150 || m_pos.x > 1174) {
        m_active = false;
    }
}

void Bullet::UpdateHoming(float deltaTime, const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<std::unique_ptr<Boss>>& bosses) {
    if (!m_active) return;
    m_lifeTime += deltaTime;

    if (m_isHoming && !m_isTargetLocked && m_lifeTime > 0.05f) {
        Entity* target = nullptr;
        float minDist = 2000.0f;

        for (auto& boss : bosses) {
            if (boss->IsActive()) {
                Vector2 bp = boss->GetPosition();
                float d = sqrtf((bp.x - m_pos.x)*(bp.x - m_pos.x) + (bp.y - m_pos.y)*(bp.y - m_pos.y));
                if (d < minDist) { minDist = d; target = boss.get(); }
            }
        }
        for (auto& enemy : enemies) {
            if (enemy->IsActive()) {
                Vector2 ep = enemy->GetPosition();
                float d = sqrtf((ep.x - m_pos.x)*(ep.x - m_pos.x) + (ep.y - m_pos.y)*(ep.y - m_pos.y));
                if (d < minDist) { minDist = d; target = enemy.get(); }
            }
        }

        if (target) {
            Vector2 tp = target->GetPosition();
            float dx = tp.x - m_pos.x;
            float dy = tp.y - m_pos.y;
            float d = sqrtf(dx*dx + dy*dy);
            if (d > 0.1f) {
                m_vx = (dx / d) * m_speed;
                m_vy = (dy / d) * m_speed;
                m_isTargetLocked = true;
            }
        }
    }

    m_pos.x += m_vx * deltaTime;
    m_pos.y += m_vy * deltaTime;

    if (m_pos.y < -150 || m_pos.y > 1174 || m_pos.x < -150 || m_pos.x > 1174) {
        m_active = false;
    }
}

void Bullet::Draw(D3D11Renderer* renderer) {
    if (!m_active || !m_pImg) return;
    
    float drawW = 32.0f;
    float drawH = 32.0f;

    switch (m_type) {
    case BulletType::PLAYER_MAIN:
        drawW = 8.0f + (m_variant - 1) * 2.5f; 
        drawH = 12.0f + (m_variant - 1) * 4.0f;
        if (m_variant == 4) { drawW = 14.0f; drawH = 22.0f; }
        break;
    case BulletType::PLAYER_VULCAN:
        drawW = 20.0f; drawH = 32.0f;
        break;
    case BulletType::PLAYER_PLASMA:
        drawW = 32.0f; drawH = 32.0f;
        break;
    case BulletType::ENEMY:
        drawW = 70.0f; drawH = 70.0f; 
        if (m_isSmall) { drawW = 22.0f; drawH = 22.0f; }
        break;
    case BulletType::BOSS:
        drawW = 32.0f; drawH = 32.0f;
        if (m_variant >= 2000) { drawW = 24.0f; drawH = 24.0f; }
        break;
    }

    drawW *= m_scale;
    drawH *= m_scale;

    // RENDERING CULLING
    if (m_pos.x < -drawW || m_pos.x > 1024 + drawW || m_pos.y < -drawH || m_pos.y > 1024 + drawH) return;

    float angleRad = m_rotationAngle * 3.14159f / 180.0f;
    if (!m_friendly) {
        angleRad += 3.14159f; // Enemy bullets face opposite direction usually
    }

    renderer->DrawTexture(m_pImg, m_pos.x, m_pos.y, drawW, drawH, angleRad, m_alpha);
}

void Bullet::UpdateOrbit(float deltaTime, Vector2 target, const std::vector<std::unique_ptr<Boss>>& bosses) {
    if (!m_active) return;
    
    m_phaseTimer += deltaTime;
    
    Boss* owner = nullptr;
    for (auto& b : bosses) {
        if (b->IsActive() && b->GetBossId() == m_orbitBossId) {
            owner = b.get();
            break;
        }
    }
    
    if (!owner) {
        // Boss died while orbiting
        m_active = false;
        return;
    }
    
    if (m_variant == 360) {
        Vector2 bossPos = owner->GetPosition();
        if (m_phase == 0) {
            // Phase 0: Directed Convergence - Move to assigned orbit slot
            float rad = m_orbitAngle * 3.14159f / 180.0f;
            Vector2 dockingPos = { bossPos.x + cosf(rad) * m_orbitRadius, bossPos.y + sinf(rad) * m_orbitRadius };
            
            float dx = dockingPos.x - m_pos.x;
            float dy = dockingPos.y - m_pos.y;
            float d = sqrtf(dx*dx + dy*dy);
            
            // Fade-in during convergence
            if (m_alpha < 1.0f) m_alpha = min(1.0f, m_alpha + deltaTime * 2.0f);

            if (d <= 15.0f) { // Increased distance to ensure docking trigger
                // Docked successfully
                m_pos = dockingPos;
                m_vx = 0.0f; m_vy = 0.0f;
                m_phase = 1; // Auto-transition to synchronized orbit once arrived
            } else {
                // Adjust velocity to point toward docking slot
                float speed = 600.0f; // Fast convergence (Matched to Game.cpp)
                m_vx = (dx / d) * speed;
                m_vy = (dy / d) * speed;
                m_pos.x += m_vx * deltaTime;
                m_pos.y += m_vy * deltaTime;
            }
            return; 
        }
        
        if (m_phase == 1) {
            // Phase 1: Synchronized Orbit (Rotation buildup)
            m_orbitAngle += m_orbitSpeed * deltaTime;
            float rad = m_orbitAngle * 3.14159f / 180.0f;
            m_pos.x = bossPos.x + cosf(rad) * m_orbitRadius;
            m_pos.y = bossPos.y + sinf(rad) * m_orbitRadius;
            m_vx = 0.0f;
            m_vy = 0.0f;
            return;
        }
        
        if (m_phase == 2) {
            // Phase 2: Outward Radial Scatter (Exploding outward from orbit angle)
            if (m_vx == 0 && m_vy == 0) { 
                float rad = m_orbitAngle * (3.14159f / 180.0f);
                float speed = 600.0f + (float)(rand() % 251); // 600 to 850 for massive blast depth
                m_vx = cosf(rad) * speed;
                m_vy = sinf(rad) * speed;
            }
            m_pos.x += m_vx * deltaTime;
            m_pos.y += m_vy * deltaTime;
            return;
        }
    }

    if (m_phaseTimer < m_orbitReleaseTimer) {
        // Orbit Phase
        m_orbitAngle += m_orbitSpeed * deltaTime;
        if (m_orbitAngle > 360.0f) m_orbitAngle -= 360.0f;
        if (m_orbitAngle < 0.0f) m_orbitAngle += 360.0f;
        
        Vector2 bossPos = owner->GetPosition();
        Vector2 center = { bossPos.x + m_orbitAnchorOffset.x, bossPos.y + m_orbitAnchorOffset.y };
        
        float rad = m_orbitAngle * 3.14159f / 180.0f;
        m_pos.x = center.x + cosf(rad) * m_orbitRadius;
        m_pos.y = center.y + sinf(rad) * m_orbitRadius;
        
        m_vx = 0.0f;
        m_vy = 0.0f;
    } else {
        // Release Phase
        if (m_vx == 0 && m_vy == 0) { 
            // [?¨í„´ ?˜ì •] ?ˆë°˜?€ ë¬´ìž‘??ë°©í–¥?¼ë¡œ ?©ë¿Œë¦¬ê³ (Scatter), ?ˆë°˜?€ ì¡°ì? ?¬ê²©(Snipe)
            if (rand() % 100 < 50) {
                // ë¬´ìž‘??ë°©í–¥ ?œì‚¬ (Chaos Scatter)
                float randomAngle = (float)(rand() % 360) * (3.14159f / 180.0f);
                float scatterSpeed = 400.0f + (float)(rand() % 251); // 400 ~ 650 ?¬ì´??ë¬´ìž‘???ë„
                m_vx = cosf(randomAngle) * scatterSpeed;
                m_vy = sinf(randomAngle) * scatterSpeed;
            }
            else {
                // ?•ë? ì¡°ì? ?¬ê²© (Precision Sniping)
                float dx = target.x - m_pos.x;
                float dy = target.y - m_pos.y;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > 0.1f) {
                    float fireSpeed = 650.0f;
                    m_vx = (dx / dist) * fireSpeed;
                    m_vy = (dy / dist) * fireSpeed;
                }
                else {
                    m_vy = 650.0f;
                }
            }
        }
        
        m_pos.x += m_vx * deltaTime;
        m_pos.y += m_vy * deltaTime;
    }
    
    if (m_pos.y < -150 || m_pos.y > 1174 || m_pos.x < -150 || m_pos.x > 1174) {
        m_active = false;
    }
}
