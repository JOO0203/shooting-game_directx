#pragma once
#include "Entity.h"

enum class ItemType {
    POWERUP,
    HEALING,
    PLASMA,
    VULCAN,
    HOMING,
    SHIELD
};

class Item : public Entity {
public:
    Item(float x, float y, ItemType type);
    virtual void Update(float deltaTime) override;
    virtual void Draw(class D3D11Renderer* renderer) override;
    virtual EntityLayer GetLayer() const override { return EntityLayer::ITEM; }

    ItemType GetType() const { return m_type; }

private:
    ItemType m_type;
    float m_speed;
};
