#include "Item.h"
#include "D3D11Renderer.h"
#include "ResourceManager.h"

Item::Item(float x, float y, ItemType type) : m_type(type), m_speed(150.0f) {
    m_active = true;
    m_pos.x = x;
    m_pos.y = y;
    m_radius = 25.0f;
}

void Item::Update(float deltaTime) {
    if (!m_active) return;
    
    // Restore falling movement for standard items. Only keep SHIELD stationary.
    if (m_type != ItemType::SHIELD) {
        m_pos.y += m_speed * deltaTime;
    }

    if (m_pos.y > 1024) m_active = false;
}

void Item::Draw(D3D11Renderer* renderer) {
    if (!m_active) return;
    std::wstring imgPath = L"../Assets/Images/FX/exhaust_01.png"; 
    float drawSize = 70.0f; 

    if (m_type == ItemType::PLASMA) {
        imgPath = L"../Assets/Images/FX/subbullet/split_0_2.png";
    } else if (m_type == ItemType::VULCAN) {
        imgPath = L"../Assets/Images/FX/subbullet/split_0_0.png";
    } else if (m_type == ItemType::HOMING) {
        imgPath = L"../Assets/Images/FX/proton_01.png";
    } else if (m_type == ItemType::SHIELD) {
        imgPath = L"../Assets/Images/Explosions/explosion_3_09.png";
        drawSize = 100.0f; 
    }

    ID3D11ShaderResourceView* img = ResourceManager::GetInstance().GetImage(imgPath);
    if (!img) return;

    renderer->DrawTexture(img, m_pos.x, m_pos.y, drawSize, drawSize);
}
