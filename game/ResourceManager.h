#pragma once
#include <windows.h>
#include <d3d11.h>
#include <string>
#include <map>
#include "Bullet.h"

class ResourceManager {
public:
    static ResourceManager& GetInstance() {
        static ResourceManager instance;
        return instance;
    }

    ID3D11ShaderResourceView* GetImage(const std::wstring& path);
    void Preload(int stage, class D3D11Renderer* renderer); 
    ID3D11ShaderResourceView* GetBulletImage(int variant, bool stage2);
    ID3D11ShaderResourceView* GetPlayerBulletImage(BulletType type, int level);
    ID3D11ShaderResourceView* GetEnemyBulletImage();
    ID3D11ShaderResourceView* GetExplosionImage(int frame);
    void Cleanup();
    void SetRenderer(class D3D11Renderer* renderer) { m_renderer = renderer; }

private:
    ResourceManager() = default;
    ~ResourceManager();
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    std::map<std::wstring, ID3D11ShaderResourceView*> m_images;
    class D3D11Renderer* m_renderer = nullptr;
};
