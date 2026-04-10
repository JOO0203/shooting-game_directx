#include "Player.h"
#include "D3D11Renderer.h"
#include "ResourceManager.h"

Player::Player() : m_speed(400.0f), m_boundW(1024), m_boundH(768), m_state(0), 
                   m_animFrame(0), m_animTimer(0.0f), m_powerLevel(1), 
                   m_isRespawning(false), m_invincibleTimer(0.0f), 
                   m_plasmaLevel(0), m_vulcanLevel(0), m_homingEnabled(false), m_isShieldActive(false) {
    m_radius = 2.5f; // Hitbox reduced from 4.0 for grazing
}

void Player::Update(float deltaTime) {
    if (m_invincibleTimer > 0) {
        m_invincibleTimer -= deltaTime;
        if (m_invincibleTimer <= 0) m_isShieldActive = false; // Reset shield flag when duration ends
    }

    if (m_isRespawning) {
        float targetY = (float)m_boundH * 0.85f;
        m_pos.y -= 250.0f * deltaTime;
        if (m_pos.y <= targetY) {
            m_pos.y = targetY;
            m_isRespawning = false;
            m_invincibleTimer = 3.0f; 
        }
    } else {
        HandleInput(deltaTime);
    }

    m_animTimer += deltaTime;
    if (m_animTimer > 0.1f) {
        m_animTimer = 0.0f;
        m_animFrame++;
    }

    if (m_pos.x < 32) m_pos.x = 32;
    if (m_pos.x > m_boundW - 32) m_pos.x = (float)m_boundW - 32;
    if (m_pos.y < 32) m_pos.y = 32;
    if (m_pos.y > m_boundH - 32) m_pos.y = (float)m_boundH - 32;
}

void Player::EnablePlasma() {
    if (m_plasmaLevel < 3) m_plasmaLevel++;
    else m_invincibleTimer = 0.5f; 
}

void Player::EnableVulcan() {
    if (m_vulcanLevel < 3) m_vulcanLevel++;
    else m_invincibleTimer = 0.5f; 
}

void Player::MaxUpgrade() {
    m_powerLevel = 6;
    m_plasmaLevel = 3;
    m_vulcanLevel = 3;
    m_homingEnabled = true;
    m_invincibleTimer = 1.0f; // Brief shield when upgrading
}

void Player::Reset() {
    m_powerLevel = 1;
    m_plasmaLevel = 0;
    m_vulcanLevel = 0;
    m_homingEnabled = false;
    m_isRespawning = false;
    m_invincibleTimer = 0.0f;
    m_active = true;
    m_animFrame = 0;
    m_animTimer = 0.0f;
    m_state = 0;
}

void Player::StartRespawn() {
    m_active = true;
    m_isRespawning = true;
    m_pos.x = (float)m_boundW / 2.0f;
    m_pos.y = (float)m_boundH + 100.0f;
}

void Player::SetInvincible(float duration) {
    m_invincibleTimer = duration;
}

void Player::HandleInput(float deltaTime) {
    m_state = 0;
    float curSpeed = m_speed;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) curSpeed = 125.0f; // Focus Mode

    if ((GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState('A') & 0x8000)) {
        m_pos.x -= curSpeed * deltaTime;
        m_state = 2;
    }
    if ((GetAsyncKeyState(VK_RIGHT) & 0x8000) || (GetAsyncKeyState('D') & 0x8000)) {
        m_pos.x += curSpeed * deltaTime;
        m_state = 1;
    }
    if ((GetAsyncKeyState(VK_UP) & 0x8000) || (GetAsyncKeyState('W') & 0x8000)) {
        m_pos.y -= curSpeed * deltaTime;
    }
    if ((GetAsyncKeyState(VK_DOWN) & 0x8000) || (GetAsyncKeyState('S') & 0x8000)) {
        m_pos.y += curSpeed * deltaTime;
    }

    if (GetAsyncKeyState('2') & 0x8000) {
        MaxUpgrade();
    }
}

void Player::Draw(D3D11Renderer* renderer) {
    if (!m_active) return;

    // Shield Visual Effect
    if (m_isShieldActive && m_invincibleTimer > 0) {
        ID3D11ShaderResourceView* sImg = ResourceManager::GetInstance().GetImage(L"../Assets/Images/Explosions/explosion_3_09.png");
        if (sImg) {
            float sSize = 100.0f;
            renderer->DrawTexture(sImg, m_pos.x, m_pos.y, sSize, sSize);
        }
    } else if (m_invincibleTimer > 0) {
        if ((int)(m_invincibleTimer * 15) % 2 == 0) return;
    }

    std::wstring imgPath = L"../Assets/Images/Player/player_b_m.png";
    switch (m_state) {
    case 1: imgPath = L"../Assets/Images/Player/player_b_r1.png"; break;
    case 2: imgPath = L"../Assets/Images/Player/player_b_l1.png"; break;
    }

    ID3D11ShaderResourceView* img = ResourceManager::GetInstance().GetImage(imgPath);
    if (img) {
        renderer->DrawTexture(img, m_pos.x, m_pos.y, 64, 64);
    }
    
    std::wstring exPath = L"../Assets/Images/FX/exhaust_0" + std::to_wstring((m_animFrame % 5) + 1) + L".png";
    ID3D11ShaderResourceView* exImg = ResourceManager::GetInstance().GetImage(exPath);
    if (exImg) {
        renderer->DrawTexture(exImg, m_pos.x, m_pos.y + 24 + 16, 32, 32); // Adjusted for translation offset in DrawTexture
    }

    // Hitbox Visualization (Only in Focus Mode)
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
        // Use a small explosion or bullet texture as a substitute for FillEllipse
        ID3D11ShaderResourceView* coreImg = ResourceManager::GetInstance().GetImage(L"../Assets/Images/FX/subbullet/split_0_2.png");
        if (coreImg) {
            renderer->DrawTexture(coreImg, m_pos.x, m_pos.y, 10, 10, 0.0f, 1.0f, {0, 1, 1, 1});
            renderer->DrawTexture(coreImg, m_pos.x, m_pos.y, 4, 4, 0.0f, 1.0f, {1, 1, 1, 1});
        }
    }
}
