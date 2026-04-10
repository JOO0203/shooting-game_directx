#pragma once
#include "Entity.h"

class Explosion : public Entity {
public:
    Explosion(float x, float y, float size = 80.0f);
    virtual void Update(float deltaTime) override;
    virtual void Draw(class D3D11Renderer* renderer) override;
    virtual EntityLayer GetLayer() const override { return EntityLayer::OTHER; }

private:
    int m_currentFrame;
    float m_frameTimer;
    int m_maxFrames;
    float m_size;
};
