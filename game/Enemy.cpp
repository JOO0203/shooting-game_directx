#include "Enemy.h"
#include "D3D11Renderer.h"
#include "ResourceManager.h"
#include <cmath>
#include <cstdlib>

Enemy::Enemy(float x, float y, EnemyType type, int stage, int curveType, float progressDelay) : 
    m_type(type), m_stage(stage), m_hp(1), m_fireTimer(0.0f), m_fireCooldown(1.5f), 
    m_speed(200.0f), m_dir(1), m_moveTimer(0.0f), 
    m_mineTimer(0.0f), m_mineCooldown(2.0f), m_mineDropped(false), m_mineDropCount(0), 
    m_curveProgress(-progressDelay), m_progressDelay(progressDelay), m_curveType(curveType), 
    m_stayTimer(0.0f), m_isLeaving(false), m_isBaby(false),
    m_burstTimer(0.0f), m_burstRemaining(0), m_burstvx(0.0f), m_burstvy(0.0f),
    m_loopAngle(0.0f), m_spiralAngle(0.0f), m_phase(0) {
    
    m_active = true;
    m_pos.x = x;
    m_pos.y = y;
    m_spawnX = x; // Store initial X as rotation center
    
    // Default trigger for entry phases (Fixed range for CURVE visibility)
    if (type == EnemyType::CURVE) m_triggerY = 250.0f;
    else m_triggerY = (float)(rand() % 250 + 100); 
    
    m_maxStayTime = 5.0f + (float)(rand() % 40) / 10.0f; 
    m_hp = 1;

    switch (m_type) {
    case EnemyType::STRAIGHT: m_speed = 300.0f; m_fireCooldown = 1.2f; m_radius = 20.0f; m_hp = 4; break;
    case EnemyType::ZAG: m_speed = 240.0f; m_fireCooldown = 1.0f; m_radius = 25.0f; m_hp = 6; break;
    case EnemyType::DIAGONAL: m_speed = 280.0f; m_fireCooldown = 0.8f; m_radius = 20.0f; m_dir = 1; m_hp = 1; break;
    case EnemyType::MINELAYER: m_hp = 20; m_speed = 150.0f; m_radius = 40.0f; m_dir = (x < 512) ? 1 : -1; m_pos.y = 150.0f; break;
    case EnemyType::MINE: m_hp = 9999; m_speed = 0.0f; m_radius = 15.0f; m_fireCooldown = 0.08f; break;
    case EnemyType::CURVE: m_speed = 300.0f; m_radius = 25.0f; m_fireCooldown = 0.6f; m_hp = 3; break;
    }
}

void Enemy::Update(float deltaTime) {
    if (!m_active) return;
    
    m_fireTimer += deltaTime;
    if (m_burstRemaining > 0) m_burstTimer += deltaTime;
    if (m_type == EnemyType::MINE) {
        m_spiralAngle += 450.0f * deltaTime;
        m_stayTimer += deltaTime;
        if (m_stayTimer > 6.0f) m_active = false; // Disappear after 6s pattern
    }

    switch (m_type) {
    case EnemyType::STRAIGHT: MoveStraight(deltaTime); break;
    case EnemyType::ZAG: MoveZag(deltaTime); break;
    case EnemyType::DIAGONAL: MoveDiagonal(deltaTime); break;
    case EnemyType::MINELAYER: MoveMineLayer(deltaTime); break;
    case EnemyType::MINE: break;
    case EnemyType::CURVE: MoveCurve(deltaTime); break;
    }

    // Bounds check
    if (m_pos.x > -500.0f) {
        if (m_pos.y > 1100 || m_pos.y < -500 || m_pos.x < -400 || m_pos.x > 1424) {
            m_active = false;
        }
    }
}

void Enemy::MoveStraight(float deltaTime) {
    if (!m_isLeaving) {
        if (m_pos.y < m_triggerY) {
            m_pos.y += m_speed * deltaTime;
        } else {
            m_stayTimer += deltaTime;
            m_pos.y = m_triggerY; 
            if (m_stayTimer >= m_maxStayTime) m_isLeaving = true;
        }
    } else {
        m_pos.y -= m_speed * 1.5f * deltaTime;
    }
}

void Enemy::MoveZag(float deltaTime) {
    if (!m_isLeaving) {
        if (m_pos.y < m_triggerY) {
            m_pos.y += m_speed * deltaTime;
        } else {
            m_stayTimer += deltaTime;
            m_pos.x += m_speed * (float)m_dir * deltaTime;
            if (m_pos.x < 50) { m_dir = 1; m_pos.x = 50.0f; }
            else if (m_pos.x > 974) { m_dir = -1; m_pos.x = 974.0f; }
            m_pos.y = m_triggerY;
            if (m_stayTimer >= m_maxStayTime) m_isLeaving = true;
        }
    } else {
        m_pos.y -= m_speed * 1.5f * deltaTime;
    }
}

void Enemy::MoveCurve(float deltaTime) {
    if (m_phase == 0) {
        m_pos.y += m_speed * 1.5f * deltaTime;
        if (m_pos.y >= m_triggerY) {
            m_pos.y = m_triggerY;
            m_phase = 1;
        }
    } else if (m_phase == 1) {
        m_stayTimer += deltaTime;
        float rotationSpeed = 5.5f;
        if (m_curveType == 1) m_loopAngle -= rotationSpeed * deltaTime;
        else m_loopAngle += rotationSpeed * deltaTime;
        float radius = 50.0f;
        m_pos.x = m_spawnX + cosf(m_loopAngle) * radius; 
        m_pos.y = m_triggerY + sinf(m_loopAngle) * radius;
    }
}

void Enemy::MoveDiagonal(float deltaTime) {
    if (m_isLeaving) {
        m_pos.y -= m_speed * 3.0f * deltaTime; // Ultra fast retreat
        return;
    }
    if (m_phase == 0) {
        m_pos.y += m_speed * deltaTime;
        if (m_pos.y >= 333.0f) {
            m_pos.y = 333.0f;
            m_phase = 1;
        }
    } else if (m_phase == 1) {
        m_pos.x += m_speed * (float)m_dir * deltaTime;
        if (m_pos.x > 1150 || m_pos.x < -150) m_active = false; 
    }
}

void Enemy::MoveMineLayer(float deltaTime) {
    if (m_isLeaving) {
        m_pos.x += m_speed * 3.0f * (float)m_dir * deltaTime; // Speed exit
        return;
    }
    m_pos.x += m_speed * (float)m_dir * deltaTime;
    if (m_pos.x < 50) { m_dir = 1; m_pos.x = 50.0f; }
    else if (m_pos.x > 974) { m_dir = -1; m_pos.x = 974.0f; }
    m_mineTimer += deltaTime;
}

bool Enemy::ShouldFire() {
    if (m_type == EnemyType::MINELAYER) return false;
    if (m_isLeaving) return false; 

    // DIAGONAL (Green Stalker) Sequential Burst Control (0.08s)
    if (m_type == EnemyType::DIAGONAL) {
        if (m_burstRemaining > 0) {
            if (m_burstTimer >= 0.08f) {
                m_burstTimer -= 0.08f;
                return true; 
            }
            return false;
        }
    }
    
    // STRAIGHT (Blue Sentinel) Sequential Burst Control (0.08s)
    if (m_type == EnemyType::STRAIGHT) {
        if (m_burstRemaining > 0) {
            if (m_burstTimer >= 0.08f) {
                m_burstTimer -= 0.08f;
                return true;
            }
            return false;
        }
    }

    if (m_fireTimer >= m_fireCooldown) {
        m_fireTimer -= m_fireCooldown;
        return true;
    }
    return false;
}

bool Enemy::ShouldDropMine() {
    if (m_type == EnemyType::MINELAYER && m_mineTimer >= m_mineCooldown) {
        m_mineTimer -= m_mineCooldown;
        return true;
    }
    return false;
}

void Enemy::Draw(D3D11Renderer* renderer) {
    if (!m_active) return;

    std::wstring imgPath = L"../Assets/Images/Enemies/enemy_1_b_m.png";
    if (m_stage == 3) {
        switch (m_type) {
        case EnemyType::STRAIGHT: imgPath = L"../Assets/Images/Enemies/Stage3/Stars (1).png"; break;
        case EnemyType::CURVE:    imgPath = L"../Assets/Images/Enemies/Stage3/Stars (2).png"; break;
        case EnemyType::ZAG:      imgPath = L"../Assets/Images/Enemies/Stage3/Stars (3).png"; break;
        case EnemyType::MINELAYER:imgPath = L"../Assets/Images/Enemies/Stage3/Stars (5).png"; break;
        case EnemyType::DIAGONAL: imgPath = L"../Assets/Images/Enemies/Stage3/Stars (4).png"; break;
        case EnemyType::MINE:     imgPath = L"../Assets/Images/Enemies/mine_22_01.png"; break;
        default:                  imgPath = L"../Assets/Images/Enemies/Stage3/Stars (1).png"; break;
        }
    }
    else {
        switch (m_type) {
        case EnemyType::STRAIGHT: imgPath = L"../Assets/Images/Enemies/enemy_1_b_m.png"; break;
        case EnemyType::ZAG:      imgPath = L"../Assets/Images/Enemies/enemy_1_r_m.png"; break;
        case EnemyType::CURVE:    imgPath = L"../Assets/Images/Enemies/enemy_2_g_m.png"; break;
        case EnemyType::DIAGONAL: imgPath = L"../Assets/Images/Enemies/enemy_2_r_m.png"; break;
        case EnemyType::MINELAYER: imgPath = L"../Assets/Images/Enemies/enemy_2_b_m.png"; break;
        case EnemyType::MINE:     imgPath = L"../Assets/Images/Enemies/mine_1_01.png"; break;
        default:                  imgPath = L"../Assets/Images/Enemies/enemy_1_b_m.png"; break;
        }
    }

    ID3D11ShaderResourceView* img = ResourceManager::GetInstance().GetImage(imgPath);
    if (!img) return;
    
    renderer->DrawTexture(img, m_pos.x, m_pos.y, 64.0f, 64.0f);
}
