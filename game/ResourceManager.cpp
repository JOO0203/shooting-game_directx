#include "ResourceManager.h"
#include "D3D11Renderer.h"
#include <string>

ResourceManager::~ResourceManager() {
    Cleanup();
}

ID3D11ShaderResourceView* ResourceManager::GetImage(const std::wstring& path) {
    if (m_images.find(path) != m_images.end()) {
        return m_images[path];
    }

    if (!m_renderer) return nullptr;

    ID3D11ShaderResourceView* srv = m_renderer->CreateTextureFromFile(path);
    if (srv) {
        m_images[path] = srv;
        return srv;
    } else {
        std::wstring msg = L"[RESOURCE ERROR] Failed to load: " + path + L"\n";
        OutputDebugStringW(msg.c_str());
        return nullptr;
    }

    return nullptr;
}

void ResourceManager::Preload(int stage, D3D11Renderer* renderer) {
    m_renderer = renderer;
    if (stage == 2) {
        for (int i = 1; i <= 28; ++i) {
            std::wstring path = L"../Assets/Images/Bubble/Bubble (" + std::to_wstring(i) + L").png";
            GetImage(path);
        }
    } else {
        for (int x = 0; x <= 3; ++x) {
            for (int y = 7; y <= 10; ++y) {
                std::wstring path = L"../Assets/Images/Bullets/split_" + std::to_wstring(x) + L"_" + std::to_wstring(y) + L".png";
                GetImage(path);
            }
        }
    }
    for (int i = 1; i <= 9; ++i) {
        GetImage(L"../Assets/Images/Explosions/explosion_3_0" + std::to_wstring(i) + L".png");
        GetImage(L"../Assets/Images/Explosions/explosion_1_0" + std::to_wstring(i) + L".png");
    }
    for (int i = 1; i <= 4; ++i) {
        GetImage(L"../Assets/Images/FX/Bullet" + std::to_wstring(i) + L".png");
        GetImage(L"../Assets/Images/FX/Bullets" + std::to_wstring(i) + L".png"); 
    }
    GetImage(L"../Assets/Images/FX/subbullet/split_0_0.png");
    GetImage(L"../Assets/Images/FX/subbullet/split_0_2.png");
    GetImage(L"../Assets/Images/Enemies/bullet/explosion_2_02.png");
}

ID3D11ShaderResourceView* ResourceManager::GetBulletImage(int variant, bool stage2) {
    if (stage2) {
        int n = ((variant % 2500) % 5000) % 20 + 1;
        if (n < 1) n = 1; if (n > 20) n = 20;
        std::wstring path = L"../Assets/Images/Bubble/Bubble (" + std::to_wstring(n) + L").png";
        return GetImage(path);
    } else {
        int v = ((variant % 2500) % 5000);
        int x = (v / 100) % 3;
        int y = (v % 4) + 7;
        std::wstring path = L"../Assets/Images/Bullets/split_" + std::to_wstring(x) + L"_" + std::to_wstring(y) + L".png";
        return GetImage(path);
    }
}

ID3D11ShaderResourceView* ResourceManager::GetPlayerBulletImage(BulletType type, int level) {
    if (type == BulletType::PLAYER_MAIN) {
        int l = (level < 1) ? 1 : (level > 4 ? 4 : level);
        std::wstring name = (l == 4) ? L"Bullets4" : L"Bullet" + std::to_wstring(l);
        return GetImage(L"../Assets/Images/FX/" + name + L".png");
    } else if (type == BulletType::PLAYER_VULCAN) {
        return GetImage(L"../Assets/Images/FX/subbullet/split_0_0.png");
    } else if (type == BulletType::PLAYER_PLASMA) {
        return GetImage(L"../Assets/Images/FX/subbullet/split_0_2.png");
    }
    return nullptr;
}

ID3D11ShaderResourceView* ResourceManager::GetEnemyBulletImage() {
    return GetImage(L"../Assets/Images/Enemies/bullet/explosion_2_02.png");
}

ID3D11ShaderResourceView* ResourceManager::GetExplosionImage(int frame) {
    int n = frame + 1;
    if (n < 1) n = 1; if (n > 9) n = 9;
    std::wstring path = L"../Assets/Images/Explosions/explosion_3_0" + std::to_wstring(n) + L".png";
    ID3D11ShaderResourceView* srv = GetImage(path);
    if (!srv) {
        path = L"../Assets/Images/Explosions/explosion_1_0" + std::to_wstring(n) + L".png";
        srv = GetImage(path);
    }
    return srv;
}

void ResourceManager::Cleanup() {
    for (auto& pair : m_images) {
        if (pair.second) pair.second->Release();
    }
    m_images.clear();
}
