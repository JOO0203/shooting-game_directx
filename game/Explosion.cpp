#include "Explosion.h"
#include "D3D11Renderer.h"
#include "ResourceManager.h"
#include <string>

Explosion::Explosion(float x, float y, float size) : m_currentFrame(0), m_frameTimer(0.0f), m_maxFrames(9), m_size(size) {
    m_pos.x = x;
    m_pos.y = y;
    m_active = true;
}

void Explosion::Update(float deltaTime) {
    if (!m_active) return;
    m_frameTimer += deltaTime;
    if (m_frameTimer > 0.04f) { // Slightly faster for punchy feel
        m_frameTimer = 0.0f;
        m_currentFrame++;
        if (m_currentFrame >= m_maxFrames) {
            m_active = false;
        }
    }
}

void Explosion::Draw(D3D11Renderer* renderer) {
    if (!m_active) return;
    
    ID3D11ShaderResourceView* img = ResourceManager::GetInstance().GetExplosionImage(m_currentFrame);
    if (img) {
        renderer->DrawTexture(img, m_pos.x, m_pos.y, m_size, m_size);
    }
}
