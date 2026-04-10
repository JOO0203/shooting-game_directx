#pragma once
#include <windows.h>

struct Vector2 {
    float x, y;
};

enum class EntityLayer { PLAYER, ENEMY, BULLET, ITEM, BOSS, OTHER };

class Entity {
public:
    Entity();
    virtual ~Entity();

    virtual void Update(float deltaTime) = 0;
    virtual void Draw(class D3D11Renderer* renderer) = 0;
    virtual EntityLayer GetLayer() const = 0;

    void SetPosition(float x, float y) { m_pos.x = x; m_pos.y = y; }
    Vector2 GetPosition() const { return m_pos; }
    
    void SetRadius(float r) { m_radius = r; }
    float GetRadius() const { return m_radius; }

    bool IsActive() const { return m_active; }
    void SetActive(bool active) { m_active = active; }

protected:
    Vector2 m_pos;
    float m_radius;
    bool m_active;
};
