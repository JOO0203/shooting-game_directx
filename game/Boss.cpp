#include "Boss.h"
#include "D3D11Renderer.h"
#include "ResourceManager.h"
#include <algorithm>
#include <cmath>

Boss::Boss(float x, float y, int stage, int bossId)
    : m_state(BossState::INTRO), m_fireTimer(0.0f), m_patternTimer(0.0f),
      m_patternIndex(0), m_deathTimer(0.0f), m_isActuallyDead(false),
      m_stage(stage), m_bossId(bossId), m_animFrame(0), m_animTimer(0.0f) {
  m_active = true;
  m_radius = (stage == 3) ? 250.0f : 120.0f;

  if (m_stage == 1)
    m_patternSequence = { 10, 11, 12, 13, 15 }; // ?�턴 15 추�?
  else if (m_stage == 2)
    m_patternSequence = {20, 21, 24, 26, 27};
  else if (m_stage == 3)
    m_patternSequence = {30, 31, 32, 33, 39};
  else
    m_patternSequence = {10, 11};

  if (m_stage == 3) {
    if (m_bossId == 1) { // Main Boss
      m_maxHpMain = 20000;
      m_maxHpL = 5000;
      m_maxHpR = 5000;
    } else { // Sub-Boss
      m_maxHpMain = 6000;
      m_maxHpL = 1500;
      m_maxHpR = 1500;
    }
    m_hpMain = m_maxHpMain;
    m_hpL = m_maxHpL;
    m_hpR = m_maxHpR;
    m_maxHp = m_maxHpMain + m_maxHpL + m_maxHpR;
    m_hp = m_maxHp;
  } else {
    if (m_stage == 1)
      m_hp = m_maxHp = 12500;
    else if (m_stage == 2)
      m_hp = m_maxHp = 20000;
    else
      m_hp = m_maxHp = 10000;

    m_hpMain = m_maxHpMain = 0;
    m_hpL = m_hpR = 0;
  }

  m_isUltimateFired = false;
  m_pos.x = x;
  m_pos.y = y;
}

void Boss::Update(float deltaTime) {
  if (!m_active)
    return;

  if (m_state == BossState::INTRO) {
    m_pos.y += 150.0f * deltaTime;
    if (m_pos.y >= 180.0f) {
      m_pos.y = 180.0f;
      m_state = BossState::ACTIVE;
      m_patternTimer = 0.0f;
      m_fireTimer = 0.0f;
    }
  } else if (m_state == BossState::ACTIVE) {
    m_patternTimer += deltaTime;

    float duration = 9.0f;
    int current = GetCurrentPattern();
    if (current == 10)
      duration = 5.0f;
    else if (current == 11)
      duration = 8.0f;
    else if (current == 12)
      duration = 6.5f;
    else if (current == 13)
      duration = 10.0f;
    else if (current == 14)
      duration = 18.0f;
    else if (current == 15) duration = 8.5f; // ?�규 ?�턴 지???�간 ?�정
    else if (current == 20) duration = 10.0f; 
    else if (current == 21)
      duration = 10.0f;
    else if (current == 22)
      duration = 12.0f;
    else if (current == 23)
      duration = 15.0f;
    else if (current == 24)
      duration = 18.0f;
    else if (current == 25)
      duration = 22.0f;
    else if (current == 26)
      duration = 22.0f;
    else if (current == 27)
      duration = 12.0f;
    else if (current == 30)
      duration = 12.0f;
    else if (current == 31)
      duration = 12.0f;
    else if (current == 32)
      duration = 15.0f;
    else if (current == 33)
      duration = 11.0f;
    else if (current == 39)
      duration = 14.0f;
    else if (current == 34)
      duration = 10.0f;
    else if (current == 35)
      duration = 28.0f;
    else if (current == 36)
      duration = 12.0f; 
    else if (current == 37)
      duration = 20.0f;
    else if (current == 38)
      duration = 27.0f;

    if (m_stage == 3) {
      // Wing Death Phase
      if (m_hpL <= 0 && m_hpR <= 0) {
        if (current < 34) {
          m_patternSequence = {34, 35, 36, 37, 38};
          m_patternIndex = 0;
          m_patternTimer = 0.0f;
          current = GetCurrentPattern();
        }
      }
    }

    // Pattern Completion Check
    if (m_patternTimer > duration) {
      m_patternTimer = 0.0f;
      if (!m_patternSequence.empty()) {
        int currentP = GetCurrentPattern();
        bool wasUltimate = (currentP == 14 || currentP == 25);
        if (wasUltimate)
          m_isUltimateFired = true;

        if (m_hp <= m_maxHp * 0.3f && !m_isUltimateFired &&
            !(m_stage == 3 && m_hpL <= 0 && m_hpR <= 0)) {
          int ultP = (m_stage == 1) ? 14 : (m_stage == 2 ? 25 : -1);
          if (ultP != -1) {
            m_patternSequence = {ultP};
            m_patternIndex = 0;
            m_fireTimer = 999.0f;
          } else {
            m_patternIndex =
                (m_patternIndex + 1) % (int)m_patternSequence.size();
            m_fireTimer = 999.0f;
          }
        } else {
          if (wasUltimate) {
            if (m_stage == 1)
              m_patternSequence = {10, 11, 12, 13, 15};
            else if (m_stage == 2)
              m_patternSequence = {20, 21, 24, 26, 27};
            else if (m_stage == 3)
              m_patternSequence = {30, 31, 32, 33, 39};
            m_patternIndex = 0;
          } else {
            if (m_stage == 3 && m_hpL <= 0 && m_hpR <= 0) {
              if (m_patternSequence.size() < 5 || m_patternSequence[0] != 34) {
                m_patternSequence = {34, 35, 36, 37, 38};
                m_patternIndex = 0;
              } else {
                m_patternIndex =
                    (m_patternIndex + 1) % (int)m_patternSequence.size();
              }
            } else {
              m_patternIndex =
                  (m_patternIndex + 1) % (int)m_patternSequence.size();
            }
          }
          m_fireTimer = 999.0f;
        }
      }
    }

    m_fireTimer += deltaTime;

    if (m_stage == 2) {
      m_animTimer += deltaTime;
      if (m_animTimer > 0.15f) {
        m_animTimer = 0.0f;
        m_animFrame = (m_animFrame + 1) % 4;
      }
    }
  } else if (m_state == BossState::DYING) {
    m_deathTimer -= deltaTime;
    if (m_deathTimer <= 0) {
      m_isActuallyDead = true;
      m_active = false;
    }
  }
}

bool Boss::ShouldFire() {
  if (m_state != BossState::ACTIVE)
    return false;

  int current = GetCurrentPattern();
  if (current == 11)
    return false;

  float duration = 9.0f;
  if (current == 10)
    duration = 5.0f;
  else if (current == 11)
    duration = 8.0f;
  else if (current == 12)
    duration = 6.5f;
  else if (current == 13)
    duration = 10.0f;
  else if (current == 14)
    duration = 18.0f;
  else if (current == 15)
    duration = 8.5f;
  else if (current == 20)
    duration = 10.0f;
  else if (current == 21)
    duration = 10.0f;
  else if (current == 22)
    duration = 12.0f;
  else if (current == 23)
    duration = 15.0f;
  else if (current == 24)
    duration = 18.0f;
  else if (current == 25)
    duration = 22.0f;
  else if (current == 26)
    duration = 22.0f;
  else if (current == 27)
    duration = 12.0f;
  else if (current == 30)
    duration = 12.0f;
  else if (current == 31)
    duration = 12.0f;
  else if (current == 32)
    duration = 15.0f;
  else if (current == 33)
    duration = 22.0f;
  else if (current == 34)
    duration = 10.0f;
  else if (current == 35)
    duration = 28.0f;
  else if (current == 36)
    duration = 12.0f;
  else if (current == 37)
    duration = 20.0f;

  float delay = (current == 25) ? 6.0f : 1.0f;
  if (m_stage == 2 && (current == 25 || current == 26) &&
      m_patternTimer > duration - delay)
    return false;

  float interval = 0.4f;
  switch (current) {
  case 10:
    interval = 0.04f;
    break;
  case 12:
    interval = 0.035f;
    break;
  case 13:
    interval = 99.0f;
    break;
  case 14: interval = 0.0f; break; // �??�레???�사�??�해 0?�로 ?�정
  case 15: interval = 0.045f; break; // ?�규 ?�턴 발사 간격
  case 20: interval = 0.05f; break;
  case 21:
    interval = 99.0f;
    break;
  case 22:
    interval = 0.08f;
    break;
  case 23:
    interval = 0.04f;
    break;
  case 24:
    interval = 0.085f;
    break; // Increased from 0.04f for clear gaps between fast bullets
  case 25:
    interval = 2.50f;
    break;
  case 26:
    interval = 0.0f;
    break;
  case 27:
    interval = 0.05f;
    break;
  case 30:
    interval = 0.12f;
    break;
  case 31:
    interval = 0.15f;
    break;
  case 32:
    interval = 0.0f;
    break;
  case 33:
    interval = 0.4f;
    break;
  case 39:
    interval = 0.0f;
    break;
  case 34:
    interval = 0.05f;
    break;
  case 35:
    interval = 0.0f;
    break;
  case 36:
    interval = 0.0f;
    break;
  case 37:
    interval = 0.06f;
    break;
  case 38:
    interval = 0.05f;
    break;
  default:
    interval = 0.4f;
    break;
  }

  if (m_fireTimer >= interval) {
    m_fireTimer = 0.0f;
    return true;
  }
  return false;
}

void Boss::Draw(D3D11Renderer *renderer) {
  if (!m_active)
    return;

  std::wstring basePath =
      L"Assets/Images/Boss/Stage" + std::to_wstring(m_stage) + L"/";

  if (m_stage == 3) {
    ID3D11ShaderResourceView *wingImg =
        ResourceManager::GetInstance().GetImage(basePath + L"Boss3-2.png");
    if (wingImg) {
      float wingW = 180.0f;
      float wingH = 180.0f;
      int currentP = GetCurrentPattern();

      if (currentP == 32) {
        float R = 400.0f;
        float f = (std::min)(1.0f, m_patternTimer / 2.0f);

        auto getV = [&](int i) {
          float a = (72.0f * (float)i - 90.0f) * (3.14159f / 180.0f);
          Vector2 targetV = {m_pos.x + cosf(a) * R, m_pos.y + sinf(a) * R};
          Vector2 startPos = m_pos;
          if (i == 2 || i == 4)
            startPos.x -= 220.0f;
          else
            startPos.x += 220.0f;
          return Vector2{startPos.x + (targetV.x - startPos.x) * f,
                         startPos.y + (targetV.y - startPos.y) * f};
        };

        if (m_hpL > 0) {
          Vector2 v2 = getV(2), v4 = getV(4);
          renderer->DrawTexture(wingImg, v2.x, v2.y, wingW, wingH);
          renderer->DrawTexture(wingImg, v4.x, v4.y, wingW, wingH);
        }
        if (m_hpR > 0) {
          Vector2 v1 = getV(1), v3 = getV(3);
          renderer->DrawTexture(wingImg, v1.x, v1.y, wingW, wingH);
          renderer->DrawTexture(wingImg, v3.x, v3.y, wingW, wingH);
        }
      } else {
        if (m_hpL > 0) {
          renderer->DrawTexture(wingImg, m_pos.x - 220.0f, m_pos.y, wingW,
                                wingH);
        }
        if (m_hpR > 0) {
          renderer->DrawTexture(wingImg, m_pos.x + 220.0f, m_pos.y, wingW,
                                wingH);
        }
      }
    }

    ID3D11ShaderResourceView *bodyImg =
        ResourceManager::GetInstance().GetImage(basePath + L"Boss3-1.png");
    if (bodyImg) {
      renderer->DrawTexture(bodyImg, m_pos.x, m_pos.y, 350.0f, 350.0f);
    }
  } else {
    std::wstring imgFile;
    if (m_stage == 1)
      imgFile =
          (m_state == BossState::DYING) ? L"BossJetdie.png" : L"BossJet.png";
    else if (m_stage == 2)
      imgFile = L"Boss2.png";
    else
      imgFile = L"BossJet.png";

    ID3D11ShaderResourceView *img =
        ResourceManager::GetInstance().GetImage(basePath + imgFile);
    if (img) {
      renderer->DrawTexture(img, m_pos.x, m_pos.y, 300.0f, 300.0f);
    }
  }
}

void Boss::TakeDamage(int damage, float bulletX) {
  if (m_state != BossState::ACTIVE)
    return;

  if (m_stage == 3) {
    int currentP = GetCurrentPattern();

    if (currentP == 32) {
      float R = 400.0f;
      float f = min(1.0f, m_patternTimer / 2.0f);

      auto getV = [&](int i) -> Vector2 {
        float a = (72.0f * (float)i - 90.0f) * (3.14159f / 180.0f);
        Vector2 targetV = {m_pos.x + cosf(a) * R, m_pos.y + sinf(a) * R};
        Vector2 startPos = m_pos;
        if (i == 2 || i == 4)
          startPos.x -= 220.0f;
        else
          startPos.x += 220.0f;
        return Vector2{startPos.x + (targetV.x - startPos.x) * f,
                       startPos.y + (targetV.y - startPos.y) * f};
      };

      bool hitAny = false;
      for (int i : {2, 4}) {
        Vector2 v = getV(i);
        if (fabsf(bulletX - v.x) < 90.0f && m_hpL > 0) {
          m_hpL -= damage;
          m_hp -= damage;
          if (m_hpL <= 0) {
            m_hpL = 0;
            m_bulletSpeedMultiplier += 0.25f;
          }
          hitAny = true;
          break;
        }
      }
      if (!hitAny) {
        for (int i : {1, 3}) {
          Vector2 v = getV(i);
          if (fabsf(bulletX - v.x) < 90.0f && m_hpR > 0) {
            m_hpR -= damage;
            m_hp -= damage;
            if (m_hpR <= 0) {
              m_hpR = 0;
              m_bulletSpeedMultiplier += 0.25f;
            }
            hitAny = true;
            break;
          }
        }
      }
      if (!hitAny && m_hpL <= 0 && m_hpR <= 0) {
        if (fabsf(bulletX - m_pos.x) < 120.0f) {
          m_hpMain -= damage;
          m_hp -= damage;
        }
      }
    } else {
      float leftBoundary = m_pos.x - 120.0f;
      float rightBoundary = m_pos.x + 120.0f;

      if (bulletX < leftBoundary) {
        if (m_hpL > 0) {
          m_hpL -= damage;
          m_hp -= damage;
          if (m_hpL <= 0) {
            m_hpL = 0;
            m_bulletSpeedMultiplier += 0.25f;
          }
        }
      } else if (bulletX > rightBoundary) {
        if (m_hpR > 0) {
          m_hpR -= damage;
          m_hp -= damage;
          if (m_hpR <= 0) {
            m_hpR = 0;
            m_bulletSpeedMultiplier += 0.25f;
          }
        }
      } else {
        if (m_hpL <= 0 && m_hpR <= 0) {
          m_hpMain -= damage;
          m_hp -= damage;
          if (m_hpMain < 0)
            m_hpMain = 0;
        }
      }
    }
  } else {
    m_hp -= damage;
  }

  if (m_hp <= 0) {
    m_hp = 0;
    m_state = BossState::DYING;
  }
}
