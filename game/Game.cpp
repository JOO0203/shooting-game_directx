#include "Game.h"
#include "D3D11Renderer.h"
#include "ResourceManager.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <string>

Game::Game()
    : m_hWnd(NULL), m_width(1024), m_height(1024), m_state(GameState::START),
      m_fireTimer(0.0f), m_spawnTimer(0.0f), m_score(0), m_playerLife(5),
      m_bgY(0.0f), m_killCount(0), m_isBossStage(false), m_isGameOver(false),
      m_stage(1), m_bgScrollSpeed(150.0f), m_gameTimer(0.0f),
      m_bossDifficulty(1.0f), m_statusMsg(L""), m_msgTimer(0.0f),
      m_prevStage(1), m_bgTransitionAlpha(1.0f), m_totalRespawns(0) {}

Game::~Game() { Cleanup(); }

bool Game::Initialize(HWND hWnd) {
  m_hWnd = hWnd;
  // VERY IMPORTANT: Clear old textures before destroying the old renderer's
  // device
  ResourceManager::GetInstance().Cleanup();

  m_renderer = std::make_unique<D3D11Renderer>();
  if (!m_renderer->Initialize(m_hWnd, m_width, m_height)) {
    MessageBox(hWnd, L"DirectX 11 초기???�패!", L"Error",
               MB_OK | MB_ICONERROR);
    return false;
  }

  ResourceManager::GetInstance().SetRenderer(m_renderer.get());
  Reset();

  QueryPerformanceFrequency(&m_qpcFrequency);
  QueryPerformanceCounter(&m_qpcLastTime);

  // Preload resources for high FPS
  ResourceManager::GetInstance().Preload(m_stage, m_renderer.get());

  m_player.Reset();
  m_player.SetPosition((float)m_width / 2.0f, (float)m_height * 0.85f);
  m_player.SetBounds(m_width, m_height);
  m_player.SetRadius(4.0f);

  m_bulletPool.assign(MAX_BULLETS, Bullet());
  m_activeBullets.clear();
  m_inactiveBullets.clear();
  m_activeBullets.reserve(MAX_BULLETS);
  m_inactiveBullets.reserve(MAX_BULLETS);
  for (int i = 0; i < MAX_BULLETS; ++i)
    m_inactiveBullets.push_back(&m_bulletPool[i]);

  m_grid = std::make_unique<SpatialGrid>(m_width, m_height, 128);

  return true;
}

void Game::Run() {
  LARGE_INTEGER currentTime;
  QueryPerformanceCounter(&currentTime);
  float deltaTime = (float)(currentTime.QuadPart - m_qpcLastTime.QuadPart) /
                    (float)m_qpcFrequency.QuadPart;

  // FPS 제한: 너무 빨리 루프가 돌면 대기 (약 70 FPS 상한)
  if (deltaTime < FIXED_DELTA_TIME * 0.8f) {
    Sleep(0); // CPU 점유율 조절 및 대기
    return;
  }

  if (deltaTime > 0.1f)
    deltaTime = 0.1f;

  m_qpcLastTime = currentTime;
  m_accumulator += deltaTime;

  while (m_accumulator >= FIXED_DELTA_TIME) {
    Update(FIXED_DELTA_TIME);
    m_accumulator -= FIXED_DELTA_TIME;
  }

  Render();

  m_fpsFrameCount++;
  m_fpsTimer += deltaTime;
  if (m_fpsTimer >= 1.0f) {
    m_currentFps = (int)((float)m_fpsFrameCount / m_fpsTimer);
    m_fpsFrameCount = 0;
    m_fpsTimer = 0.0f;
  }
}

void Game::Update(float deltaTime) {
  if (m_state == GameState::START) {
    if ((GetAsyncKeyState(VK_RETURN) & 0x8000) ||
        (GetAsyncKeyState(VK_SPACE) & 0x8000)) {
      m_state = GameState::PLAYING;
    }
    return;
  }

  if (m_playerLife <= 0 && !m_player.IsRespawning()) {
    m_state = GameState::GAMEOVER;
  }

  if (m_state == GameState::GAMEOVER) {
    if ((GetAsyncKeyState('1') & 0x8000)) {
      Reset();
      m_state = GameState::START;
    } else if ((GetAsyncKeyState('2') & 0x8000)) {
      m_playerLife = 5;
      m_isGameOver = false;
      m_player.SetPosition((float)m_width / 2.0f, (float)m_height * 0.85f);
      m_state = GameState::PLAYING;
    }
    return;
  }

  if (m_state == GameState::ENDING) {
    m_endingTimer -= deltaTime;
    if (rand() % 100 < 15) {
      float rx = (float)(rand() % m_width);
      float ry = (float)(rand() % m_height);
      m_explosions.push_back(
          std::make_unique<Explosion>(rx, ry, (float)(rand() % 150 + 100)));
    }
    if (m_endingTimer <= 0) {
      m_state = GameState::GAMECLEAR;
      m_msgTimer = 5.0f; // Start 5s countdown
    }
    return;
  }

  if (m_state == GameState::GAMECLEAR) {
    m_msgTimer -= deltaTime;
    if (m_msgTimer <= 0) {
      Initialize(m_hWnd);
    }
    return;
  }

  if (m_state == GameState::PLAYING) {
    // --- Debug Tool: Key '3' (33% Boss HP Damage) ---
    static bool k3Down = false;
    if (GetAsyncKeyState('3') & 0x8000) {
      if (!k3Down) {
        for (auto &boss : m_bosses) {
          if (boss->IsActive()) {
            Bullet *dbgB = GetFreeBullet();
            if (dbgB) {
              Vector2 pPos = m_player.GetPosition();
              // Fire standard-looking bullet with debug damage variant
              dbgB->Fire(pPos.x, pPos.y, 0.0f, -2500.0f, true,
                         BulletType::PLAYER_MAIN, 250);
              dbgB->SetImage(ResourceManager::GetInstance().GetImage(
                  L"Assets/Images/Bullet/bullet_2-3.png"));
              dbgB->SetScale(1.0f);
            }
          }
        }
        k3Down = true;
      }
    } else {
      k3Down = false;
    }

    m_enemiesToAdd.clear();
    m_itemsToAdd.clear();
    Vector2 pp = m_player.GetPosition(); // Define early for bullet patterns
    m_player.Update(deltaTime);
    if (m_bossHitEffectTimer > 0)
      m_bossHitEffectTimer -= deltaTime;
    m_gameTimer += deltaTime;
    m_bgY += m_bgScrollSpeed * deltaTime;

    if (m_msgTimer > 0)
      m_msgTimer -= deltaTime;

    if (m_gameTimer >= 120.0f) {
      if (!m_isBossStage) {
        m_isBossStage = true;
        int bId = (m_stage == 3) ? 1 : 0; // Stage 3 Main Boss = ID 1
        m_bosses.push_back(std::make_unique<Boss>((float)m_width / 2.0f,
                                                  -200.0f, m_stage, bId));
        m_statusMsg = L"WARNING: BOSS IS APPROACHING!";
        m_msgTimer = 4.0f;
      }
    } else {
      m_waveTimer += deltaTime;
      if (m_waveTimer >= 8.0f) {
        m_waveTimer = 0.0f;
        m_waveIndex = (m_waveIndex + 1) % 5;
        m_waveSpawnCount = 0;
      }
      SpawnPattern(deltaTime);
    }

    if (!m_bosses.empty()) {
      for (auto it = m_bosses.begin(); it != m_bosses.end();) {
        auto &boss = *it;
        boss->Update(deltaTime);
        if (m_stage == 3) {
          Vector2 bp = boss->GetPosition();
          int cp = boss->GetCurrentPattern();
          float t = boss->GetPatternTimer();

          // Function to calculate wing positions for Pattern 32 lerp
          auto getLerpPos = [&](int i) {
            float a = (72.0f * i - 90.0f) * (3.14159f / 180.0f);
            Vector2 targetV(bp.x + cosf(a) * 400.0f, bp.y + sinf(a) * 400.0f);
            Vector2 startPos = bp;
            if (i == 2 || i == 4)
              startPos.x -= 220.0f;
            else
              startPos.x += 220.0f;
            float f = min(1.0f, t / 2.0f);
            return Vector2(startPos.x + (targetV.x - startPos.x) * f,
                           startPos.y + (targetV.y - startPos.y) * f);
          };

          if (boss->CheckAndResetWingLExplosion()) {
            Vector2 lpos =
                (cp == 32) ? getLerpPos(2) : Vector2(bp.x - 220, bp.y);
            for (int i = 0; i < 12; ++i) {
              m_explosions.push_back(std::make_unique<Explosion>(
                  lpos.x + (rand() % 120 - 60), lpos.y + (rand() % 120 - 60)));
            }
            FireSubBossDeathPattern(lpos);
          }
          if (boss->CheckAndResetWingRExplosion()) {
            Vector2 rpos =
                (cp == 32) ? getLerpPos(1) : Vector2(bp.x + 220, bp.y);
            for (int i = 0; i < 12; ++i) {
              m_explosions.push_back(std::make_unique<Explosion>(
                  rpos.x + (rand() % 120 - 60), rpos.y + (rand() % 120 - 60)));
            }
            FireSubBossDeathPattern(rpos);
          }

          // Pattern 34: Supernova Collapse Logic (Suction & Core Growth)
          if (cp == 34) {
            float pt = boss->GetPatternTimer();
            if (pt < 4.0f) {
              // 1. Suction on Player
              Vector2 pp_local = m_player.GetPosition();
              float dx = bp.x - pp_local.x, dy = bp.y - pp_local.y;
              float dist = sqrtf(dx * dx + dy * dy);
              if (dist > 1.0f) {
                float suctionLimit = 300.0f;
                float force = (1.0f - min(1.0f, dist / 900.0f)) *
                              320.0f; // Increased force (from 120) and range
                m_player.SetPosition(
                    pp_local.x + (dx / dist) * force * deltaTime,
                    pp_local.y + (dy / dist) * force * deltaTime);
              }

              // 2. Absorb side bullets to grow core
              for (auto b : m_activeBullets) {
                if (b->IsActive() && !b->IsFriendly() &&
                    b->GetVariant() ==
                        340) { // Variant 340 is the 'Inward Star'
                  Vector2 bPos = b->GetPosition();
                  float bDx = bp.x - bPos.x, bDy = bp.y - bPos.y;
                  float bDist2 = bDx * bDx + bDy * bDy;
                  if (bDist2 < 50.0f * 50.0f) {
                    b->SetActive(false);
                    m_supernovaScale += 0.015f; // Growth
                  }
                }
              }
            } else {
              // Expansion phase handled in FireBossBullet or here if needed for
              // visuals
            }
          }

          // Pattern 35: Celestial Branding (Telegraph Check)
          if (cp == 35) {
            float pt = boss->GetPatternTimer();
            for (auto b : m_activeBullets) {
              if (b->IsActive() && !b->IsFriendly() &&
                  b->GetVariant() == 351) { // Branding Telegraph
                float age = b->GetLifeTime();
                // Fade in/out for telegraph
                if (age < 0.3f)
                  b->SetAlpha(age / 0.3f);
                else if (age > 1.2f)
                  b->SetAlpha(max(0.0f, (1.5f - age) / 0.3f));
                else
                  b->SetAlpha(1.0f);

                // --- New: Flickering Visuals (44/45 toggle) ---
                // Speed up flicker over time: 0.35s down to 0.04s
                float flickerInterval = 0.35f - (age / 1.5f) * 0.31f;
                if (flickerInterval < 0.04f)
                  flickerInterval = 0.04f;
                int frameIdx = (int)(age / flickerInterval);
                std::wstring corePath = (frameIdx % 2 == 0) ? L"45" : L"44";
                b->SetImage(ResourceManager::GetInstance().GetImage(
                    L"Assets/Images/EnergyBullet/Energy (" + corePath +
                    L").png"));
                b->SetScale(5.5f); // Increased size

                if (age >= 1.5f) {
                  b->SetActive(false);
                  // 2. Explosion & Burst
                  Vector2 bp_brand = b->GetPosition();
                  m_explosions.push_back(std::make_unique<Explosion>(
                      bp_brand.x, bp_brand.y, 180.0f));

                  for (int i = 0; i < 110; ++i) { // Hundreds of particles
                    float angle =
                        (float)i * (360.0f / 110.0f) * (3.14159f / 180.0f);
                    Bullet *frag = GetFreeBullet();
                    if (frag) {
                      float spd = 200.0f + (rand() % 400);
                      frag->Fire(bp_brand.x, bp_brand.y, cosf(angle) * spd,
                                 sinf(angle) * spd, false, BulletType::BOSS);
                      int starId = 1 + (rand() % 18);
                      frag->SetImage(ResourceManager::GetInstance().GetImage(
                          L"Assets/Images/StarBullet/Star (" +
                          std::to_wstring(starId) + L").png"));
                      frag->SetScale(0.6f);
                      frag->SetRadius(6.0f);
                    }
                  }
                }
              }

              // Fade logic for Orbiting bullets (from user request)
              if (b->IsActive() && b->IsOrbiting() &&
                  boss->GetCurrentPattern() == 35) {
                Vector2 bpos = b->GetPosition();
                float alpha = 1.0f;
                // Fade out when leaving the visible top arc (Y < 50)
                if (bpos.y < 50.0f)
                  alpha = max(0.0f, (bpos.y + 100.0f) / 150.0f);
                b->SetAlpha(alpha);
                if (bpos.y < -150.0f)
                  b->SetActive(false); // Extend cleanup for stream
              }
            }
          }
        }
        if (boss->ShouldFire())
          FireBossBullet(boss.get(), deltaTime);
        if (boss->GetState() == BossState::DYING) {
          if (rand() % 100 < 40) {
            float ofx = (float)(rand() % 240 - 120);
            float ofy = (float)(rand() % 240 - 120);
            if (m_stage == 3) {
              ofx *= 2.5f;
              ofy *= 1.5f;
            }
            m_explosions.push_back(std::make_unique<Explosion>(
                boss->GetPosition().x + ofx, boss->GetPosition().y + ofy));
          }
        }
        if (boss->IsActuallyDead()) {
          m_score += 50000;
          m_itemsToAdd.push_back(std::make_unique<Item>(
              boss->GetPosition().x, boss->GetPosition().y, ItemType::HOMING));
          it = m_bosses.erase(it);
        } else {
          ++it;
        }
      }

      if (m_bosses.empty()) {
        m_isBossStage = false;
        if (m_stage >= 3) {
          m_state = GameState::ENDING;
          m_endingTimer = 4.0f;
        } else {
          m_prevStage = m_stage;
          m_stage++;
          m_bgTransitionAlpha = 0.0f;
          m_bgScrollSpeed += 100.0f;
          m_bossDifficulty += 0.3f;
          m_gameTimer = 0.0f;
          m_statusMsg = L"STAGE " + std::to_wstring(m_stage) + L" START";
          m_msgTimer = 3.5f;
        }
      }
    }

    for (auto it = m_enemies.begin(); it != m_enemies.end();) {
      (*it)->Update(deltaTime);
      if ((*it)->ShouldFire()) {
        if ((*it)->GetType() == EnemyType::MINE) {
          float ang = (*it)->GetSpiralAngle();
          float rad = ang * 3.14159f / 180.0f;
          float spd = 300.0f;
          Bullet *b = GetFreeBullet();
          if (b) {
            b->Fire((*it)->GetPosition().x, (*it)->GetPosition().y,
                    cosf(rad) * spd, sinf(rad) * spd, false, BulletType::ENEMY);
            b->SetImage(ResourceManager::GetInstance().GetEnemyBulletImage());
          }
          // Dual spiral for better coverage
          b = GetFreeBullet();
          if (b) {
            b->Fire((*it)->GetPosition().x, (*it)->GetPosition().y,
                    cosf(rad + 3.14159f) * spd, sinf(rad + 3.14159f) * spd,
                    false, BulletType::ENEMY);
            b->SetImage(ResourceManager::GetInstance().GetEnemyBulletImage());
          }
        } else
          FireEnemyBullet(it->get());
      }
      if ((*it)->ShouldDropMine() && (*it)->GetType() == EnemyType::MINELAYER &&
          !(*it)->HasDroppedMine()) {
        m_enemiesToAdd.push_back(std::make_unique<Enemy>(
            (*it)->GetPosition().x, (*it)->GetPosition().y, EnemyType::MINE,
            m_stage));
        int count = (*it)->GetMineDropCount() + 1;
        (*it)->SetMineDropCount(count);
        if (count >= 2) {
          (*it)->SetMineDropped(true); // Stop dropping
          (*it)->SetIsLeaving(true);   // Start exit maneuver
        }
      }
      if (!(*it)->IsActive()) {
        it = m_enemies.erase(it);
      } else
        ++it;
    }
    if (!m_enemiesToAdd.empty()) {
      for (auto &e : m_enemiesToAdd)
        m_enemies.push_back(std::move(e));
      m_enemiesToAdd.clear();
    }

    for (auto it = m_items.begin(); it != m_items.end();) {
      (*it)->Update(deltaTime);
      if (!(*it)->IsActive()) {
        it = m_items.erase(it);
      } else
        ++it;
    }

    for (auto b : m_activeBullets) {
      if (b->IsHoming())
        b->UpdateHoming(deltaTime, m_enemies, m_bosses);
      else if (b->IsPattern())
        b->UpdatePattern(deltaTime, pp);
      else if (b->IsOrbiting())
        b->UpdateOrbit(deltaTime, pp, m_bosses);
      else
        b->Update(deltaTime);
    }

    static float turretFireTimer = 0.0f;
    turretFireTimer += deltaTime;

    // Turret-based Patterns (13 and 14)
    std::vector<Vector2> turretLocs;
    bool bossInTurretPattern = false;
    int activePattern = 0;
    float twistAngle = 0.0f;
    for (auto &boss : m_bosses) {
      if (boss->IsActive() && boss->GetState() == BossState::ACTIVE) {
        int p = boss->GetCurrentPattern();
        if (p == 13) {
          twistAngle = boss->GetPatternTimer() *
                       160.0f; // ?�복 ?�동?�서 지???�전 방식?�로 변�?
          if (boss->GetPatternTimer() <
              7.5f) { // ?�턴 종료 2.5�???발사 중단 (?�환 ?�멸 ?�간 ?�보)
            bossInTurretPattern = true;
            activePattern = 13;
          }
        } else if (p == 14) {
          // ?�턴 14: ?�버로드 - ?�탄 발사
          if (boss->GetPatternTimer() <
              15.0f) { // 18�?�?15초까지�?발사 (3초간 ?�장 ?�간 ?�보)
            bossInTurretPattern = false;
            activePattern = 14;
          }
        } else if (p == 21) {
          // Pattern 21: Devil's Eye (Targeting turrets)
          bossInTurretPattern = true;
          activePattern = 21;
        }
      }
    }

    for (auto b : m_activeBullets) {
      if (b->IsActive() && b->IsTurret()) {
        if (bossInTurretPattern) {
          Vector2 pos = b->GetPosition();
          turretLocs.push_back(pos);
        } else {
          b->SetActive(false); // Cleanup immediately if pattern ends
        }
      }
    }
    static float turretSpiralAngle = 0.0f;
    static float turretSpiralTimer = 0.0f;
    turretSpiralTimer += deltaTime;

    if (activePattern == 21 && turretSpiralTimer > 0.12f) {
      turretSpiralTimer = 0.0f;
      turretSpiralAngle += 35.0f;
      for (auto &pos : turretLocs) {
        Bullet *sb = GetFreeBullet();
        if (sb) {
          float rad = turretSpiralAngle * 3.14159f / 180.0f;
          sb->Fire(pos.x, pos.y, cosf(rad) * 150.0f, sinf(rad) * 150.0f, false,
                   BulletType::BOSS);
          sb->SetImage(ResourceManager::GetInstance().GetImage(
              L"Assets/Images/EnergyBullet/Energy (9).png"));
          sb->SetScale(0.55f);
        }
      }
    }

    float turretInterval =
        (activePattern == 21) ? 1.5f : 0.50f; // Slowed from 0.14f
    if (turretFireTimer > turretInterval) {
      turretFireTimer = 0.0f;
      for (auto &pos : turretLocs) {
        if (activePattern == 21) {
          // Devil's Eye: Aimed 3-way shot at player
          Vector2 target = m_player.GetPosition();
          float dx = target.x - pos.x, dy = target.y - pos.y;
          float baseAngle = atan2f(dy, dx);
          float offsets[] = {-10.0f, 0.0f, 10.0f};
          for (float off : offsets) {
            Bullet *b = GetFreeBullet();
            if (b) {
              float angle = baseAngle + (off * 3.14159f / 180.0f);
              float bs = 250.0f;
              b->Fire(pos.x, pos.y, cosf(angle) * bs, sinf(angle) * bs, false,
                      BulletType::BOSS);
              int imgId = (rand() % 10) + 1;
              b->SetImage(ResourceManager::GetInstance().GetImage(
                  L"Assets/Images/EnergyBullet/Energy (" +
                  std::to_wstring(imgId) + L").png"));
              b->SetScale(0.85f);
            }
          }
        } else {
          int bulletCount = (activePattern == 14)
                                ? 6
                                : 10; // ?�환 ?�성???�??증�? (2->6, 4->10)
          for (int dir = 0; dir < bulletCount; ++dir) {
            float step = 360.0f / (float)bulletCount;
            float angle = (twistAngle + (float)dir * step) * 3.14159f / 180.0f;
            Bullet *b = GetFreeBullet();
            if (b) {
              float bs =
                  (activePattern == 14)
                      ? 40.0f
                      : 180.0f; // Buffed to 40.0f for better visual "bloom"
              b->Fire(pos.x, pos.y, cosf(angle) * bs, sinf(angle) * bs, false,
                      BulletType::BOSS);

              if (activePattern == 14) {
                // Pattern 14: Accumulate for 3 seconds then rush player with
                // acceleration
                // --- New: Staggered Rush Logic ---
                // Find which turret (group) this bullet came from
                int pulseGroup = 0;
                // We'll need to find the turret that fired this.
                // In this loop context, 'pos' is the turret's position.
                // We can use the variant of the turret to determine the pulse
                // group. Let's look for the turret at this position.
                for (auto tur : m_activeBullets) {
                  if (tur->IsActive() && tur->IsTurret() &&
                      fabs(tur->GetPosition().x - pos.x) < 2.0f &&
                      fabs(tur->GetPosition().y - pos.y) < 2.0f) {
                    pulseGroup = tur->GetVariant() - 140;
                    break;
                  }
                }

                b->SetPattern(true);
                b->SetAccelPattern(true);
                // Stagger the start of the "rush" phase by 1.5 seconds per
                // group (Increased from 1.3s for clarity)
                b->SetPhaseTimer(-1.8f - (float)(pulseGroup * 1.5f));
                int cId = (rand() % 15) + 1;
                b->SetImage(ResourceManager::GetInstance().GetImage(
                    L"Assets/Images/EnergyBullet/colorEnergy (" +
                    std::to_wstring(cId) + L").png"));
                b->SetScale(0.8f);
              } else {
                // Using split_x_y from Bullets folder (x:0~3, y:7~10)
                int randX = rand() % 4;
                int randY = (rand() % 4) + 7;
                b->SetImage(ResourceManager::GetInstance().GetImage(
                    L"Assets/Images/Bullets/split_" +
                    std::to_wstring(randX) + L"_" + std::to_wstring(randY) +
                    L".png"));
              }
            }
          }
        }
      }
    }

    for (auto it = m_explosions.begin(); it != m_explosions.end();) {
      (*it)->Update(deltaTime);
      if (!(*it)->IsActive()) {
        it = m_explosions.erase(it);
      } else
        ++it;
    }

    if (m_player.IsActive() && !m_player.IsRespawning()) {
      m_fireTimer += deltaTime;
      if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
        if (m_fireTimer >= 0.12f) {
          m_fireTimer = 0.0f;
          FirePlayerBullet();
        }
      }
      static float pt = 0, vt = 0;
      pt += deltaTime;
      vt += deltaTime;
      if (m_player.GetPlasmaLevel() > 0 && pt > 0.3f) {
        pt = 0;
        FireAutoSubWeapon(true);
      }
      if (m_player.GetVulcanLevel() > 0 && vt > 0.12f) {
        vt = 0;
        FireAutoSubWeapon(false);
      }
    }

    if (m_bgTransitionAlpha < 1.0f) {
      m_bgTransitionAlpha += deltaTime * 0.5f;
      if (m_bgTransitionAlpha > 1.0f)
        m_bgTransitionAlpha = 1.0f;
    }

    static float skipCooldown = 0.0f;
    if (skipCooldown > 0)
      skipCooldown -= deltaTime;
    if ((GetAsyncKeyState('Q') & 0x8000) && skipCooldown <= 0) {
      m_gameTimer += 60.0f;
      skipCooldown = 0.5f;
    }
    if ((GetAsyncKeyState('1') & 0x8000) && skipCooldown <= 0 &&
        !m_bosses.empty()) {
      for (auto &b : m_bosses) {
        b->TakeDamage(999999, b->GetPosition().x - 200.0f); // Left Wing
        b->TakeDamage(999999, b->GetPosition().x + 200.0f); // Right Wing
        b->TakeDamage(999999, b->GetPosition().x);          // Main Body
      }
      skipCooldown = 0.5f;
    }

    CheckCollisions();

    for (size_t i = 0; i < m_activeBullets.size();) {
      if (!m_activeBullets[i]->IsActive()) {
        m_inactiveBullets.push_back(m_activeBullets[i]);
        m_activeBullets[i] = m_activeBullets.back();
        m_activeBullets.pop_back();
      } else {
        ++i;
      }
    }
  }
}

void Game::SpawnPattern(float deltaTime) {
  m_spawnIntervalTimer += deltaTime;

  // ?�환??2.5�?증�? ?�용 (?��??�을 2.5�??�눔)
  float spawnIntervalScale = (1.0f / (1.0f + (m_stage - 1) * 0.5f)) / 2.5f;

  switch (m_waveIndex) {
  case 0: // WAVE 0: DIAGONAL AMBUSH (?�환???�향: 12 -> 30)
    if (m_spawnIntervalTimer >= (0.3f * spawnIntervalScale) &&
        m_waveSpawnCount < 30) {
      m_spawnIntervalTimer = 0.0f;
      // Spawn inside the screen to ensure immediate visibility
      float startX = 200.0f + (m_waveSpawnCount * 60.0f);
      auto enemy = std::make_unique<Enemy>(startX, -50.0f, EnemyType::DIAGONAL,
                                           m_stage, 0, 0.0f);
      m_enemies.push_back(std::move(enemy));
      m_waveSpawnCount++;
    }
    break;
  case 1: // WAVE 1: SYMMETRICAL CURVE DUO (1.5s interval)
  {
    int curveCount = 0;
    for (auto &e : m_enemies)
      if (e->IsActive() && e->GetType() == EnemyType::CURVE)
        curveCount++;
    if (curveCount >= 2)
      return; // Limit active curves to 2

    if (m_spawnIntervalTimer >= (1.5f * spawnIntervalScale) &&
        m_waveSpawnCount < 6) {
      m_spawnIntervalTimer = 0.0f;
      float x = (m_waveSpawnCount == 0) ? 212.0f : 812.0f;
      m_enemies.push_back(std::make_unique<Enemy>(
          x, -50.0f, EnemyType::CURVE, m_stage, m_waveSpawnCount, 0.0f));
      m_waveSpawnCount++;
    }
  } break;
  case 2: // WAVE 2: HYBRID VANGUARD (?�환???�향: 3 -> 8)
    if (m_spawnIntervalTimer >= (2.0f * spawnIntervalScale) &&
        m_waveSpawnCount < 8) {
      m_spawnIntervalTimer = 0.0f;
      if (m_waveSpawnCount < 5) {
        float x = (m_waveSpawnCount == 0) ? 200.0f : 824.0f;
        m_enemies.push_back(
            std::make_unique<Enemy>(x, -50.0f, EnemyType::STRAIGHT, m_stage));
      } else {
        m_enemies.push_back(
            std::make_unique<Enemy>(512.0f, -50.0f, EnemyType::ZAG, m_stage));
      }
      m_waveSpawnCount++;
    }
    break;
  case 3: // WAVE 3: INTENSE CROSSING (2 x CURVE pairs)
  {
    int curveCount = 0;
    for (auto &e : m_enemies)
      if (e->IsActive() && e->GetType() == EnemyType::CURVE)
        curveCount++;
    if (curveCount >= 2)
      return; // Limit active curves to 2

    if (m_spawnIntervalTimer >= (1.5f * spawnIntervalScale) &&
        m_waveSpawnCount < 10) {
      m_spawnIntervalTimer = 0.0f;
      float x = (m_waveSpawnCount % 2 == 0) ? 150.0f : 874.0f;
      m_enemies.push_back(std::make_unique<Enemy>(
          x, -50.0f, EnemyType::CURVE, m_stage, m_waveSpawnCount % 2));
      m_waveSpawnCount++;
    }
  } break;
  case 4: // WAVE 4: VOID DREADNOUGHT (?�환???�향)
    if (m_stage >= 2 && m_spawnIntervalTimer >= (2.0f * spawnIntervalScale) &&
        m_waveSpawnCount < 3) {
      m_spawnIntervalTimer = 0.0f;
      float side = (rand() % 2 == 0) ? -100.0f : 1124.0f; // Spawn from sides
      m_enemies.push_back(
          std::make_unique<Enemy>(side, 200.0f, EnemyType::MINELAYER, m_stage));
      m_waveSpawnCount++;
    } else if (m_stage < 2 &&
               m_spawnIntervalTimer >= (1.0f * spawnIntervalScale) &&
               m_waveSpawnCount < 3) {
      // Fallback for stage 1 if wave 4 reached
      m_spawnIntervalTimer = 0.0f;
      m_enemies.push_back(std::make_unique<Enemy>(
          (float)(rand() % 800 + 112), -50.0f, EnemyType::ZAG, m_stage));
      m_waveSpawnCount++;
    }
    break;
  }
}

void Game::FirePlayerBullet() {
  int level = m_player.GetPowerLevel();
  Vector2 p = m_player.GetPosition();
  auto fr = [&](float ox, float oy, float vx, float vy) {
    Bullet *b = GetFreeBullet();
    if (b) {
      b->Fire(ox, oy, vx, vy, true, BulletType::PLAYER_MAIN, level);
      b->SetImage(ResourceManager::GetInstance().GetPlayerBulletImage(
          BulletType::PLAYER_MAIN, level));
    }
  };
  switch (level) {
  case 1:
    fr(p.x, p.y - 32.0f, 0.0f, -850.0f);
    break;
  case 2:
    fr(p.x - 10.0f, p.y - 32.0f, 0.0f, -850.0f);
    fr(p.x + 10.0f, p.y - 32.0f, 0.0f, -850.0f);
    break;
  case 3:
    fr(p.x, p.y - 32.0f, 0.0f, -850.0f);
    fr(p.x - 14.0f, p.y - 25.0f, -100.0f, -800.0f);
    fr(p.x + 14.0f, p.y - 25.0f, 100.0f, -800.0f);
    break;
  case 4:
    fr(p.x - 7.0f, p.y - 32.0f, 0.0f, -850.0f);
    fr(p.x + 7.0f, p.y - 32.0f, 0.0f, -850.0f);
    fr(p.x - 20.0f, p.y - 20.0f, -150.0f, -800.0f);
    fr(p.x + 20.0f, p.y - 20.0f, 150.0f, -800.0f);
    break;
  case 5:
    fr(p.x, p.y - 32.0f, 0.0f, -900.0f);
    fr(p.x - 18.0f, p.y - 30.0f, -80.0f, -850.0f);
    fr(p.x + 18.0f, p.y - 30.0f, 80.0f, -850.0f);
    fr(p.x - 35.0f, p.y - 20.0f, -200.0f, -800.0f);
    fr(p.x + 35.0f, p.y - 20.0f, 200.0f, -800.0f);
    break;
  case 6:
    fr(p.x - 5.0f, p.y - 32.0f, 0.0f, -950.0f);
    fr(p.x + 5.0f, p.y - 32.0f, 0.0f, -950.0f);
    fr(p.x - 15.0f, p.y - 30.0f, 0.0f, -900.0f);
    fr(p.x + 15.0f, p.y - 30.0f, 0.0f, -900.0f);
    fr(p.x - 30.0f, p.y - 25.0f, 0.0f, -850.0f);
    fr(p.x + 30.0f, p.y - 25.0f, 0.0f, -850.0f);
    fr(p.x - 45.0f, p.y - 20.0f, 0.0f, -800.0f);
    fr(p.x + 45.0f, p.y - 20.0f, 0.0f, -800.0f);
    break;
  }
}

void Game::FireAutoSubWeapon(bool isPlasma) {
  Vector2 p = m_player.GetPosition();
  int level = isPlasma ? m_player.GetPlasmaLevel() : m_player.GetVulcanLevel();
  auto fr = [&](float ox, float vx, float vy) {
    Bullet *b = GetFreeBullet();
    BulletType bt =
        isPlasma ? BulletType::PLAYER_PLASMA : BulletType::PLAYER_VULCAN;
    bool isHoming = (level >= 3 && m_player.IsHomingEnabled());
    if (b) {
      b->Fire(p.x + ox, p.y, vx, vy, true, bt, 0, isHoming);
      b->SetImage(
          ResourceManager::GetInstance().GetPlayerBulletImage(bt, level));
    }
  };
  float wingOffset = 45.0f;
  if (isPlasma) {
    if (level == 1)
      fr(-wingOffset, -200, -700);
    else if (level == 2) {
      fr(-wingOffset, -200, -700);
      fr(wingOffset, 200, -700);
    } else if (level >= 3) {
      fr(-wingOffset, -300, -650);
      fr(0, 0, -700);
      fr(wingOffset, 300, -650);
    }
  } else {
    if (level == 1)
      fr(0, 0, -800);
    else if (level == 2) {
      fr(-wingOffset, 0, -800);
      fr(wingOffset, 0, -800);
    } else if (level >= 3) {
      fr(-wingOffset, 0, -800);
      fr(0, 0, -800);
      fr(wingOffset, 0, -800);
    }
  }
}

void Game::FireEnemyBullet(Enemy *enemy) {
  Vector2 ep = enemy->GetPosition();
  EnemyType type = enemy->GetType();

  // 1. EXCLUSIVE PRIORITY: Green Stalker (DIAGONAL) 8-Shot FIXED Aimed Stream
  if (type == EnemyType::DIAGONAL) {
    if (enemy->GetBurstRemaining() == 0) {
      // Initial Aim: Capture the player's position ONLY at the start of the
      // burst
      Vector2 pp = m_player.GetPosition();
      float dx = pp.x - ep.x, dy = pp.y - ep.y;
      float dist = sqrtf(dx * dx + dy * dy);
      if (dist > 0.1f) {
        float burstSpeed = 550.0f;
        enemy->SetBurstvx((dx / dist) * burstSpeed);
        enemy->SetBurstvy((dy / dist) * burstSpeed);
        enemy->SetBurstRemaining(11);
      }
    } else {
      enemy->SetBurstRemaining(enemy->GetBurstRemaining() - 1);
      if (enemy->GetBurstRemaining() == 0) {
        enemy->SetIsLeaving(true); // Pattern complete after 8th shot
      }
    }

    // Fire all shots in the burst using the FIXED velocity vector captured at
    // start
    Bullet *b = GetFreeBullet();
    if (b) {
      b->Fire(ep.x, ep.y, enemy->GetBurstvx(), enemy->GetBurstvy(), false,
              BulletType::ENEMY);
      b->SetPattern(true); // Phase 1: 1.2s Gather, Phase 2: One-time Re-aim
      b->SetPhaseTimer(0.0f);
      b->SetIsSmall(true); // Enable micro-bullet rendering (45x45)
      b->SetRadius(3.0f);  // Reduced hitbox for precision Danmaku
      b->SetImage(ResourceManager::GetInstance().GetImage(
          L"Assets/Images/Enemies/bullet/split_1_3.png"));
    }
    return; // Important: Exit early for DIAGONAL
  }

  auto fr = [&](float vx, float vy) {
    Bullet *b = GetFreeBullet();
    if (b) {
      b->Fire(ep.x, ep.y, vx, vy, false, BulletType::ENEMY);
      b->SetImage(ResourceManager::GetInstance().GetEnemyBulletImage());
    }
  };

  if (type == EnemyType::STRAIGHT) {
    if (enemy->GetBurstRemaining() == 0) {
      Vector2 pp = m_player.GetPosition();
      float dx = pp.x - ep.x, dy = pp.y - ep.y;
      float dist = sqrtf(dx * dx + dy * dy);
      if (dist > 0.1f) {
        float streamSpeed = 480.0f;
        enemy->SetBurstvx((dx / dist) * streamSpeed);
        enemy->SetBurstvy((dy / dist) * streamSpeed);
        enemy->SetBurstRemaining(4);
        fr(enemy->GetBurstvx(), enemy->GetBurstvy());
      }
    } else {
      fr(enemy->GetBurstvx(), enemy->GetBurstvy());
      enemy->SetBurstRemaining(enemy->GetBurstRemaining() - 1);
    }
  } else if (type == EnemyType::ZAG) {
    Vector2 pp = m_player.GetPosition();
    float dx = pp.x - ep.x, dy = pp.y - ep.y;
    float baseAngle = atan2f(dy, dx);
    float speed = 300.0f;
    for (int i = -3; i <= 3; ++i) {
      float angle = baseAngle + (i * 7.0f) * (3.14159f / 180.0f);
      fr(cosf(angle) * speed, sinf(angle) * speed);
    }
  } else if (type == EnemyType::CURVE) {
    float speed = 200.0f;
    for (int i = 0; i < 16; ++i) {
      float angle = (i * 22.5f) * (3.14159f / 180.0f);
      fr(cosf(angle) * speed, sinf(angle) * speed);
    }
  }
}

void Game::FireBossBullet(Boss *boss, float deltaTime) {
  if (!boss)
    return;
  Vector2 bp = boss->GetPosition();
  int pattern = boss->GetCurrentPattern();
  int variant = 0;
  if (m_stage == 2) {
    // Diversify Bubble colors: 1, 6, 11, 16
    variant = 1 + (pattern % 4) * 5;
  } else {
    // Diversify Bullets: staggered color (x) and style (y)
    variant = (pattern % 3) * 100 + (7 + (pattern % 4));
  }

  auto fr = [&](float vx, float vy, int variantOverride = 0) {
    Bullet *b = GetFreeBullet();
    if (b) {
      int finalV = variantOverride > 0 ? variantOverride : variant;
      b->Fire(bp.x, bp.y, vx, vy, false, BulletType::BOSS, finalV);
      std::wstring path = L"";
      if (m_stage == 1) {
        int cId = (pattern * 3 + (finalV % 5)) % 18 + 1;
        path = L"Assets/Images/RealBullet/BossBullet (" +
               std::to_wstring(cId) + L").png";
      } else if (m_stage == 2) {
        int cId = (pattern * 5 + (finalV % 10)) % 45 + 1;
        path = L"Assets/Images/EnergyBullet/Energy (" +
               std::to_wstring(cId) + L").png";
      } else if (m_stage == 3) {
        int cId = (pattern * 3 + (finalV % 5)) % 18 + 1;
        path = L"Assets/Images/StarBullet/Star (" + std::to_wstring(cId) +
               L").png";
      }

      if (!path.empty())
        b->SetImage(ResourceManager::GetInstance().GetImage(path));
      else
        b->SetImage(ResourceManager::GetInstance().GetBulletImage(
            finalV, m_stage == 2));
    }
  };
  float bs = 200.0f * m_bossDifficulty; // ?꾩냽
  int den = (int)(m_bossDifficulty * 2);
  switch (pattern) {
  case 10: {
    // ?�턴 10: ?�개 조�? ?�격 �?궤도 ?�환 ?�치
    float t = boss->GetPatternTimer();
    float burstCycle = 0.5f;
    Vector2 pp = m_player.GetPosition();

    if (fmodf(t, burstCycle) < burstCycle * 0.6f) {
      for (int side = -1; side <= 1; side += 2) {
        Vector2 firePos = {bp.x + (side * 140.0f), bp.y};
        float dx = pp.x - firePos.x, dy = pp.y - firePos.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > 0.1f) {
          float a = atan2f(dy, dx);
          Bullet *b = GetFreeBullet();
          if (b) {
            b->Fire(firePos.x, firePos.y, cosf(a) * bs * 3.8f,
                    sinf(a) * bs * 3.8f, false, BulletType::BOSS);
            b->SetImage(ResourceManager::GetInstance().GetImage(
                L"Assets/Images/RealBullet/BossBullet (7).png"));
          }
        }
      }
    }

    static int spawnCount = 0;
    spawnCount++;
    for (int side = -1; side <= 1; side += 2) {
      Bullet *b = GetFreeBullet();
      if (b) {
        Vector2 anchor(side * 140.0f, -10.0f);
        b->Fire(bp.x + anchor.x, bp.y + anchor.y, 0.0f, 0.0f, false,
                BulletType::BOSS);
        // ?�턴 11?�서 ?�나???�차?�으�??��??�록 ?�?�머 ?�정
        // 가변 리듬 ?�용: ?�차??증�????�인???�프?�을 ?�해 ?�이 뭉치�?
        // ?�어지???�과 ?�출
        float baseInterval = (float)(spawnCount % 24) * 0.18f;
        float rhythmicOffset = sinf((float)spawnCount * 0.6f) * 0.45f;
        float releaseTimer = 6.0f + baseInterval + rhythmicOffset;
        float startAngle = (side == -1) ? (spawnCount * 18.0f)
                                        : (180.0f - (spawnCount * 18.0f));
        float rotSpeed = (side == -1) ? -170.0f : 170.0f;
        b->SetOrbit(true, boss->GetBossId(), 75.0f, rotSpeed, startAngle,
                    releaseTimer, anchor);
        int imgId = (rand() % 18) + 1;
        b->SetImage(ResourceManager::GetInstance().GetImage(
            L"Assets/Images/RealBullet/BossBullet (" +
            std::to_wstring(imgId) + L").png"));
      }
    }
  } break;
  case 11: {
    // ?�턴 11: ??개�?�?(강화?? - ?�실???�각??변??보장
    float t = boss->GetPatternTimer();
    Vector2 pp = m_player.GetPosition();

    // 1?�계: 급속 ?�력 충전 (0.0s ~ 1.2s) - ?�개 주�????�환?�이 빠르�?
    // ?�용?�이치며 ?�성??
    if (t < 1.2f) {
      if (fmodf(t, 0.04f) < deltaTime) {
        for (int side = -1; side <= 1; side += 2) {
          Bullet *b = GetFreeBullet();
          if (b) {
            Vector2 wingPos(side * 140.0f, 0.0f);
            b->Fire(bp.x + wingPos.x, bp.y + wingPos.y, 0.0f, 0.0f, false,
                    BulletType::BOSS);

            // 1.2�??�후 즉시 방출 ?�작 (?�차??
            float releaseTimer = 1.2f + (t * 0.15f);
            b->SetOrbit(true, boss->GetBossId(), 65.0f,
                        (side == -1 ? -320.0f : 320.0f), t * 360.0f,
                        releaseTimer, wingPos);
            int imgId = ((int)(t * 100) % 18) + 1;
            b->SetImage(ResourceManager::GetInstance().GetImage(
                L"Assets/Images/RealBullet/BossBullet (" +
                std::to_wstring(imgId) + L").png"));
            b->SetScale(0.9f);
          }
        }
      }
    }
    // 2?�계: 직접 ?�사 가??(1.2s ~ 종료) - 모아???�환 ?�격�?별개�?본체?�서
    // 직접 ?�트�??�격 ?�행
    else {
      if (fmodf(t, 0.045f) < deltaTime) {
        for (int side = -1; side <= 1; side += 2) {
          Vector2 firePos = {bp.x + (side * 140.0f), bp.y + 10.0f};
          float dx = pp.x - firePos.x, dy = pp.y - firePos.y;
          float dist = sqrtf(dx * dx + dy * dy);
          if (dist > 1.0f) {
            float a = atan2f(dy, dx);
            Bullet *b = GetFreeBullet();
            if (b) {
              float speed = bs * 5.2f;
              b->Fire(firePos.x, firePos.y, cosf(a) * speed, sinf(a) * speed,
                      false, BulletType::BOSS);
              b->SetImage(ResourceManager::GetInstance().GetImage(
                  L"Assets/Images/RealBullet/BossBullet (12).png"));
              b->SetScale(0.95f);
            }
          }
        }
      }
    }
  } break;
  case 12: {
    // ?�턴 12: ?�???�속 ?�윕
    float t = boss->GetPatternTimer();
    float cycle = 1.5f;
    float angleOffset = cosf(t * (2.0f * 3.14159f / cycle)) * 45.0f;
    for (int side = -1; side <= 1; side += 2) {
      Vector2 wing(side * 90.0f, 0.0f);
      for (int spread = 0; spread < 2; ++spread) {
        Bullet *b = GetFreeBullet();
        if (b) {
          float outdoorOffset = (spread == 1) ? (side * 28.0f) : 0.0f;
          float fireAngleDeg = 90.0f - (side * angleOffset) + outdoorOffset;
          float rad = fireAngleDeg * 3.14159f / 180.0f;
          b->Fire(bp.x + wing.x, bp.y + wing.y, cosf(rad) * bs * 1.8f,
                  sinf(rad) * bs * 1.8f, false,
                  BulletType::BOSS); // ?�도 ?�향 (2.2f -> 1.8f)
          b->SetImage(ResourceManager::GetInstance().GetImage(
              L"Assets/Images/RealBullet/BossBullet (2).png"));
        }
      }
    }
  } break;
  case 13: {
    for (auto b : m_activeBullets) {
      if (b->IsTurret())
        b->SetActive(false);
    }
    float angles[] = {0.0f, 30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 180.0f};
    for (float deg : angles) {
      Bullet *b = GetFreeBullet();
      if (b) {
        float rad = deg * 3.14159f / 180.0f;
        b->Fire(bp.x, bp.y, cosf(rad) * 400.0f, sinf(rad) * 400.0f, false,
                BulletType::BOSS);
        b->SetPattern(true);
        b->SetPhaseTimer(0.0f);
        b->SetTurret(true);
        b->SetImage(ResourceManager::GetInstance().GetImage(
            L"Assets/Images/EnergyBullet/Energy (4).png"));
        b->SetScale(1.8f);
      }
    }
  } break;
  case 14: {
    // [Stage 1 ?�살�? ?�버로드: ?�탄 발사 (Overload: Full Barrage)
    float t = boss->GetPatternTimer();
    Vector2 pp = m_player.GetPosition();

    // 1. ?�도 미사??(???�개 - 주기??발사)
    static float missileTimer = 0.0f;
    missileTimer += deltaTime;
    if (missileTimer > 0.6f) {
      missileTimer = 0.0f;
      for (int side = -1; side <= 1; side += 2) {
        Bullet *mb = GetFreeBullet();
        if (mb) {
          Vector2 firePos = {bp.x + (side * 145.0f), bp.y + 20.0f};
          mb->Fire(firePos.x, firePos.y, 0.0f, 210.0f, false, BulletType::BOSS,
                   0, true); // ?�도 추�? ?�향
          int imgId = 1 + (rand() % 45);
          mb->SetImage(ResourceManager::GetInstance().GetImage(
              L"Assets/Images/EnergyBullet/Energy (" +
              std::to_wstring(imgId) + L").png"));
          mb->SetScale(1.2f);
        }
      }
    }

    // 2. 규칙?�인 ?�용?�이 ?�막 (8갈래 Spiral)
    // 방향 ?�환 ?�수 ?�향 (??4.5초마??방향 반전, �?4??반복)
    int arms = 8;
    float cycle = 3.0f; // ?�버로드 주기�???빠르�?(4.5 -> 3.0)
    float phase = fmodf(t, cycle);

    // [?�정] ?�격 중�? ?�간??�?0.2�??�후 0.1�?�??�향?�여 긴장�??�향
    bool isPausing = (phase < 0.1f || phase > cycle - 0.1f ||
                      fabsf(phase - cycle * 0.5f) < 0.1f);

    float rotationAmount = (phase < (cycle * 0.5f)) ? phase : (cycle - phase);

    if (!isPausing) { // ?�격 중�? 구간???�닐 ?�만 발사
      for (int i = 0; i < arms; ++i) {
        Bullet *b = GetFreeBullet();
        if (b) {
          // ?�전 ?�도�?추�? ?�향 (650 -> 800)?�여 ?�욱 ?�카로운 ?�용?�이 ?�출
          float baseAngle =
              ((rotationAmount * 800.0f) + (i * (360.0f / arms))) *
              (3.14159f / 180.0f);
          float angle = baseAngle;
          float speed = 250.0f; // ?�속 ?�향: 300 -> 250

          b->Fire(bp.x, bp.y + 40.0f, cosf(angle) * speed, sinf(angle) * speed,
                  false, BulletType::BOSS);

          int imgId = (i % 18) + 1;
          b->SetImage(ResourceManager::GetInstance().GetImage(
              L"Assets/Images/RealBullet/BossBullet (" +
              std::to_wstring(imgId) + L").png"));
          b->SetScale(0.9f);

          // 3. ?�자(Particle) ?�과: 본체?� ?�벽???�기?�된 ?�상
          if (rand() % 100 < 50) {
            Bullet *p = GetFreeBullet();
            if (p) {
              float pSpeed = speed * 0.9f;
              p->Fire(bp.x, bp.y + 40.0f, cosf(angle) * pSpeed,
                      sinf(angle) * pSpeed, false, BulletType::BOSS);
              p->SetNoDamage(true);
              p->SetAlpha(0.5f);
              p->SetScale(0.5f);
              p->SetLifeTime(0.7f);
              int energyId = 1 + (rand() % 45);
              p->SetImage(ResourceManager::GetInstance().GetImage(
                  L"Assets/Images/EnergyBullet/Energy (" +
                  std::to_wstring(energyId) + L").png"));
            }
          }
        }
      }
    }
  } break;
  case 15: {
    // [?�턴 ?�설�? 거�? X-?�트?�이???�벽 (X-Strike Wall)
    float t = boss->GetPatternTimer();

    // 좌우 ?�덤 30???�동 ?�출 (sin�??�덤?�을 ?�어 부?�러???�들�?구현)
    float globalTilt =
        sinf(t * 0.8f) * 30.0f; // 초당 ??30??범위 ?�에??좌우�??�동

    // 보스 몸체 중앙???�닌 ?�쪽?�로 ?�게 ?�진 ?�치?�서 발사?�여 ?�벽 ?�성
    for (int i = 0; i < 2; ++i) { // ??겹의 ?�벽
      for (int side = -1; side <= 1; side += 2) {
        // 각도 ?�정: 기본 60??120??기반??globalTilt ?�용
        float baseAng = (side == -1) ? 65.0f : 115.0f;
        float finalDeg = baseAng + globalTilt;
        float rad = finalDeg * 3.14159f / 180.0f;

        // ?��? ?�성 범위 ?�보
        for (float offsetX = -200.0f; offsetX <= 200.0f; offsetX += 100.0f) {
          Bullet *b = GetFreeBullet();
          if (b) {
            Vector2 spawnPos = {bp.x + offsetX, bp.y - 20.0f};
            float speed = 380.0f;
            b->Fire(spawnPos.x, spawnPos.y, cosf(rad) * speed,
                    sinf(rad) * speed, false, BulletType::BOSS);

            // RealBullet ?��?지 ?�용
            int imgId = (side == -1) ? 1 : 12; // 줄기 방향별로 ?�상 구분
            b->SetImage(ResourceManager::GetInstance().GetImage(
                L"Assets/Images/RealBullet/BossBullet (" +
                std::to_wstring(imgId) + L").png"));
            b->SetScale(1.2f);
          }
        }
      }
    }
  } break;
  case 20: {
    // Stage 2 Pattern 20: Spiral Hell
    float t = boss->GetPatternTimer();
    // Variable rotation speed using sin function
    static float cumulativeTwist = 0.0f;
    float twistSpeed = (360.0f + sinf(t * 1.5f) * 200.0f);
    cumulativeTwist += twistSpeed * deltaTime;

    for (int i = 0; i < 12; ++i) { // 줄기 ???�향 (7 -> 12)
      float angle =
          (cumulativeTwist + i * (360.0f / 12.0f)) * 3.14159f / 180.0f;
      Bullet *b = GetFreeBullet();
      if (b) {
        float bs = 220.0f;
        b->Fire(bp.x, bp.y, cosf(angle) * bs, sinf(angle) * bs, false,
                BulletType::BOSS);
        // ?�청?�신 Energy (17~25) ?�리�??�용
        int imgId = 17 + (i % 9);
        b->SetImage(ResourceManager::GetInstance().GetImage(
            L"Assets/Images/EnergyBullet/Energy (" + std::to_wstring(imgId) +
            L").png"));
        b->SetScale(1.3f); // ?�너지 ?�환??가?�성???�해 ?��????�폭 ?�향
      }
    }
  } break;
  case 21: {
    // Stage 2 Pattern 21: Devil's Eye
    // Cleanup potential lingering turrets
    for (auto b : m_activeBullets) {
      if (b->IsActive() && b->IsTurret())
        b->SetActive(false);
    }

    // Spawn 8 fixed turrets in circular formation
    for (int i = 0; i < 8; ++i) {
      float angle = (i * 45.0f) * 3.14159f / 180.0f;
      Bullet *b = GetFreeBullet();
      if (b) {
        float dist = 250.0f;
        float tx = bp.x + cosf(angle) * dist;
        float ty = bp.y + sinf(angle) * dist;
        float spawnSpeed = 300.0f;
        b->Fire(bp.x, bp.y, cosf(angle) * spawnSpeed, sinf(angle) * spawnSpeed,
                false, BulletType::BOSS);
        b->SetPattern(true); // Will stop
        b->SetTurret(true);
        b->SetPhaseTimer(0.0f); // Initial fast -> slow -> stop
        b->SetImage(ResourceManager::GetInstance().GetImage(
            L"Assets/Images/EnergyBullet/Energy (1).png"));
        b->SetScale(3.2f); // Increased from 2.5f for visual impact
      }
    }
  } break;
  case 24: {
    // Stage 2 Pattern 24: Combined Cross and Rain (Final Phase)
    float t = boss->GetPatternTimer();

    static float cumulativeCrossAngle = 0.0f;
    cumulativeCrossAngle += 260.0f * deltaTime;

    // --- Removed Escape Window Logic (Back to Continuous Fire) ---
    float crossAngles[] = {0.0f, 90.0f, 180.0f, 270.0f};
    for (float base : crossAngles) {
      float rad = (base + cumulativeCrossAngle) * 3.14159f / 180.0f;
      Bullet *b = GetFreeBullet();
      if (b) {
        float bs = 750.0f; // Increased speed for high-speed cross
        b->Fire(bp.x, bp.y, cosf(rad) * bs, sinf(rad) * bs, false,
                BulletType::BOSS);
        int imgId = 14 + (rand() % 3);
        b->SetImage(ResourceManager::GetInstance().GetImage(
            L"Assets/Images/EnergyBullet/Energy (" + std::to_wstring(imgId) +
            L").png"));
        b->SetScale(1.8f);
      }
    }

    // --- FIXED: Rain Spawn (Time-based instead of frame-based) ---
    static float rainTimer = 0.0f;
    rainTimer += deltaTime;
    if (rainTimer >=
        0.055f) { // Approximately 18 bullets per second, consistent density
      rainTimer = 0.0f;
      float startX = (float)(rand() % (m_width - 200)) + 100.0f;
      Bullet *rb = GetFreeBullet();
      if (rb) {
        float vy = 200.0f + (float)(rand() % 150);
        rb->Fire(startX, -30.0f, 0.0f, vy, false, BulletType::BOSS);
        if (rand() % 100 < 20) {
          rb->SetPattern(true);
          rb->SetAccelPattern(true);
          rb->SetPhaseTimer(-1.2f);
        }
        int pool[] = {17, 18, 19, 20, 21, 22, 23, 24,
                      25, 36, 37, 40, 41, 44, 45};
        int imgId = pool[rand() % (sizeof(pool) / sizeof(int))];
        rb->SetImage(ResourceManager::GetInstance().GetImage(
            L"Assets/Images/EnergyBullet/Energy (" + std::to_wstring(imgId) +
            L").png"));
        rb->SetScale(0.6f + (float)(rand() % 10) * 0.1f);
      }
    }

    static float fanBurstTimer = 0.0f;
    fanBurstTimer += deltaTime;
    if (fanBurstTimer > 1.2f) { // Removed isEscapeWindow check
      fanBurstTimer = 0.0f;
      float crossAngles[] = {0.0f, 90.0f, 180.0f, 270.0f};
      for (float base : crossAngles) {
        float centerRad = (base + cumulativeCrossAngle) * 3.14159f / 180.0f;
        float tipX = bp.x + cosf(centerRad) * 320.0f;
        float tipY = bp.y + sinf(centerRad) * 320.0f;
        for (int i = -2; i <= 2; ++i) {
          Bullet *b = GetFreeBullet();
          if (b) {
            float angle = centerRad + (i * 12.0f) * 3.14159f / 180.0f;
            b->Fire(tipX, tipY, cosf(angle) * 180.0f, sinf(angle) * 180.0f,
                    false, BulletType::BOSS);
            int imgId = 26 + (rand() % 9);
            b->SetImage(ResourceManager::GetInstance().GetImage(
                L"Assets/Images/EnergyBullet/Energy (" +
                std::to_wstring(imgId) + L").png"));
            b->SetScale(0.7f);
          }
        }
      }
    }
  } break;

  case 25: {
    // Stage 2 Pattern 25: Demonic Heartbeat - Left/Right Toggle (4 Pulses)
    float t = boss->GetPatternTimer();
    static float lastHbStartTime = -10.0f;
    static int hbCount = 0;

    // Reset counter when pattern restarts
    if (t < 0.2f && lastHbStartTime > 1.0f) {
      hbCount = 0;
    }
    lastHbStartTime = t;

    if (hbCount >= 4)
      break; // Only 4 pulses (L-R-L-R)

    // Toggle Offset: Left(-250), Right(+250)
    float xOffset = (hbCount % 2 == 0) ? -250.0f : 250.0f;
    Vector2 customCenter = {bp.x + xOffset, bp.y};

    int counts[] = {24, 9, 24}; // Second ring reduced from 18 to 9
    float dists[] = {40.0f, 160.0f, 300.0f};
    float phases[] = {0.0f, 2.1f, 4.2f};

    for (int layer = 0; layer < 3; ++layer) {
      for (int i = 0; i < counts[layer]; ++i) {
        float angle = (i * (360.0f / counts[layer])) * 3.14159f / 180.0f;
        Bullet *b = GetFreeBullet();
        if (b) {
          b->Fire(customCenter.x, customCenter.y, 0.0f, 0.0f, false,
                  BulletType::BOSS);
          b->SetHeartbeat(true, customCenter, dists[layer], angle,
                          phases[layer]);

          if (layer == 0) {
            int imgId = 44 + (rand() % 2);
            b->SetImage(ResourceManager::GetInstance().GetImage(
                L"Assets/Images/EnergyBullet/Energy (" +
                std::to_wstring(imgId) + L").png"));
            b->SetScale(1.3f);
          } else {
            int imgId = 2 + (rand() % 9);
            b->SetImage(ResourceManager::GetInstance().GetImage(
                L"Assets/Images/EnergyBullet/Energy (" +
                std::to_wstring(imgId) + L").png"));
            b->SetScale(0.7f + (layer * 0.25f));
          }
        }
      }
    }
    hbCount++;
  } break;

  case 26: {
    // Stage 2 Pattern 26: 7s Prep + 2s Delay + 20s Cage + 6s Final Delay
    float t = boss->GetPatternTimer();

    static bool startedCharge = false;
    if (t < 0.1f)
      startedCharge = false;

    if (t < 7.0f) {
      if (!startedCharge) {
        startedCharge = true;
        Bullet *bL = GetFreeBullet();
        if (bL) {
          bL->Fire(bp.x - 150.0f, bp.y + 50.0f, 0.0f, 0.0f, false,
                   BulletType::BOSS);
          bL->SetImage(ResourceManager::GetInstance().GetImage(
              L"Assets/Images/EnergyBullet/Energy (1).png"));
          bL->SetScale(0.1f);
          bL->SetGrowthRate((6.0f - 0.1f) / 7.0f);
          bL->SetLifeTime(7.0f);
          bL->SetRotationRate(540.0f); // Fast spin
        }
        Bullet *bR = GetFreeBullet();
        if (bR) {
          bR->Fire(bp.x + 150.0f, bp.y + 50.0f, 0.0f, 0.0f, false,
                   BulletType::BOSS);
          bR->SetImage(ResourceManager::GetInstance().GetImage(
              L"Assets/Images/EnergyBullet/Energy (1).png"));
          bR->SetScale(0.1f);
          bR->SetGrowthRate((6.0f - 0.1f) / 7.0f);
          bR->SetLifeTime(7.0f);
          bR->SetRotationRate(-540.0f); // Fast spin
        }
      }
    }

    static float lastFiredT = -1.0f;
    static int firedCount = 0;
    if (t < 0.1f) {
      firedCount = 0;
      lastFiredT = -1.0f;
    }

    if (t >= 7.0f && firedCount < 3 &&
        t > lastFiredT + 0.35f) { // 간격 ?�축 (0.4 -> 0.35)
      firedCount++;
      lastFiredT = t;

      float vx = 0.0f;
      if (firedCount == 1)
        vx = 350.0f;
      else if (firedCount == 2)
        vx = 175.0f;
      else if (firedCount == 3)
        vx = 0.0f;

      Bullet *bL = GetFreeBullet();
      if (bL) {
        bL->Fire(bp.x - 150.0f, bp.y + 50.0f, vx, 750.0f, false,
                 BulletType::BOSS); // ?�속 ?�향 (600 -> 750)
        bL->SetImage(ResourceManager::GetInstance().GetImage(
            L"Assets/Images/EnergyBullet/Energy (1).png"));
        bL->SetScale(6.0f);
        bL->SetRadius(80.0f);
        bL->SetNoDamage(false);
        bL->SetRotationRate(540.0f);
      }
      Bullet *bR = GetFreeBullet();
      if (bR) {
        bR->Fire(bp.x + 150.0f, bp.y + 50.0f, -vx, 750.0f, false,
                 BulletType::BOSS); // ?�속 ?�향 (600 -> 750)
        bR->SetImage(ResourceManager::GetInstance().GetImage(
            L"Assets/Images/EnergyBullet/Energy (1).png"));
        bR->SetScale(6.0f);
        bR->SetRadius(80.0f);
        bR->SetNoDamage(false);
        bR->SetRotationRate(-540.0f);
      }
    }

    float cageT = t - 9.0f;
    static int lastStep = -1;
    if (cageT < 0.0f)
      lastStep = -1;

    if (cageT >= 0.0f) {
      int cageStep = (int)(cageT / 0.65f); // 진행 주기 가??(0.8 -> 0.65)
      if (cageStep < 16 && cageStep > lastStep) {
        lastStep = cageStep;

        float cy = m_height * 0.75f;
        float cx = m_width * 0.5f;

        struct Region {
          float x1, y1, x2, y2;
        };
        Region regions[] = {{50, cy - 100, 250, cy + 100},
                            {m_width - 250, cy - 100, m_width - 50, cy + 100},
                            {cx - 100, cy - 100, cx + 100, cy + 100},
                            {bp.x - 100, m_height * 0.5f - 100, bp.x + 100,
                             m_height * 0.5f + 100}};

        int currentSet = cageStep / 4;
        int subStep = cageStep % 4;

        if (subStep == 0) {
          Region r = regions[currentSet];
          for (float x = r.x1; x <= r.x2 + 5; x += 40) {
            for (int yDir = 0; yDir < 2; ++yDir) {
              Bullet *b = GetFreeBullet();
              if (b) {
                b->Fire(x, (yDir == 0 ? r.y1 : r.y2), 0.0f, 0.0f, false,
                        BulletType::BOSS);
                b->SetImage(ResourceManager::GetInstance().GetImage(
                    L"Assets/Images/EnergyBullet/colorEnergy (11).png"));
                b->SetNoDamage(true);
                b->SetScale(1.5f);
                b->SetLifeTime(2.5f); // ?�속 ?�상???�른 ?�명 ?�축 (4.0 -> 2.5)
              }
            }
          }
          for (float y = r.y1; y <= r.y2 + 5; y += 40) {
            for (int xDir = 0; xDir < 2; ++xDir) {
              Bullet *b = GetFreeBullet();
              if (b) {
                b->Fire((xDir == 0 ? r.x1 : r.x2), y, 0.0f, 0.0f, false,
                        BulletType::BOSS);
                b->SetImage(ResourceManager::GetInstance().GetImage(
                    L"Assets/Images/EnergyBullet/colorEnergy (10).png"));
                b->SetNoDamage(true);
                b->SetScale(1.5f);
                b->SetLifeTime(2.5f);
              }
            }
          }
        } else if (subStep == 2) {
          Region r = regions[currentSet];
          float speed = 900.0f; // ?�격 ?�도 ?�향 (850 -> 900)

          if (currentSet == 0) { // 1: Left, L->R
            for (int y = 0; y < (int)m_height; y += 45) {
              Bullet *b = GetFreeBullet();
              if (b) {
                b->Fire(-50.0f, (float)y, speed, 0.0f, false, BulletType::BOSS);
                b->SetImage(ResourceManager::GetInstance().GetImage(
                    L"Assets/Images/EnergyBullet/Energy (39).png"));
                b->SetScale(1.8f);
                b->SetRadius(25.0f);
                b->SetNoDamage(false);
                b->SetLifeTime(0.0f);
                if (y > r.y1 - 25 && y < r.y2 + 25) {
                  b->SetLifeTime(fabsf(r.x1 - (-50.0f)) / speed);
                  Bullet *b2 = GetFreeBullet();
                  if (b2) {
                    b2->Fire(r.x2 + 20.0f, (float)y, speed, 0.0f, false,
                             BulletType::BOSS);
                    b2->SetImage(ResourceManager::GetInstance().GetImage(
                        L"Assets/Images/EnergyBullet/Energy (39).png"));
                    b2->SetScale(1.8f);
                    b2->SetRadius(25.0f);
                    b2->SetNoDamage(false);
                    b2->SetLifeTime(0.0f);
                    b2->SetSpawnDelay(fabsf((r.x2 + 20.0f) - (-50.0f)) / speed);
                  }
                }
              }
            }
          } else if (currentSet == 1) { // 2: Right, R->L
            for (int y = 0; y < (int)m_height; y += 45) {
              Bullet *b = GetFreeBullet();
              if (b) {
                b->Fire(m_width + 50.0f, (float)y, -speed, 0.0f, false,
                        BulletType::BOSS);
                b->SetImage(ResourceManager::GetInstance().GetImage(
                    L"Assets/Images/EnergyBullet/Energy (35).png"));
                b->SetScale(1.8f);
                b->SetRadius(25.0f);
                b->SetNoDamage(false);
                b->SetLifeTime(0.0f);
                if (y > r.y1 - 25 && y < r.y2 + 25) {
                  b->SetLifeTime(fabsf((m_width + 50.0f) - r.x2) / speed);
                  Bullet *b2 = GetFreeBullet();
                  if (b2) {
                    b2->Fire(r.x1 - 20.0f, (float)y, -speed, 0.0f, false,
                             BulletType::BOSS);
                    b2->SetImage(ResourceManager::GetInstance().GetImage(
                        L"Assets/Images/EnergyBullet/Energy (35).png"));
                    b2->SetScale(1.8f);
                    b2->SetRadius(25.0f);
                    b2->SetNoDamage(false);
                    b2->SetLifeTime(0.0f);
                    b2->SetSpawnDelay(
                        fabsf((m_width + 50.0f) - (r.x1 - 20.0f)) / speed);
                  }
                }
              }
            }
          } else if (currentSet == 2) { // 3: Center, B->T
            for (int x = 0; x < (int)m_width; x += 45) {
              Bullet *b = GetFreeBullet();
              if (b) {
                b->Fire((float)x, m_height + 50.0f, 0.0f, -speed, false,
                        BulletType::BOSS);
                b->SetImage(ResourceManager::GetInstance().GetImage(
                    L"Assets/Images/EnergyBullet/Energy (43).png"));
                b->SetScale(1.8f);
                b->SetRadius(25.0f);
                b->SetNoDamage(false);
                b->SetLifeTime(0.0f);
                if (x > r.x1 - 25 && x < r.x2 + 25) {
                  b->SetLifeTime(fabsf((m_height + 50.0f) - r.y2) / speed);
                  Bullet *b2 = GetFreeBullet();
                  if (b2) {
                    b2->Fire((float)x, r.y1 - 20.0f, 0.0f, -speed, false,
                             BulletType::BOSS);
                    b2->SetImage(ResourceManager::GetInstance().GetImage(
                        L"Assets/Images/EnergyBullet/Energy (43).png"));
                    b2->SetScale(1.8f);
                    b2->SetRadius(25.0f);
                    b2->SetNoDamage(false);
                    b2->SetLifeTime(0.0f);
                    b2->SetSpawnDelay(
                        fabsf((m_height + 50.0f) - (r.y1 - 20.0f)) / speed);
                  }
                }
              }
            }
          } else if (currentSet == 3) { // 4: Boss Below, Crossfire
            for (int y = 0; y < (int)m_height; y += 45) {
              bool inBox = (y > r.y1 - 25 && y < r.y2 + 25);
              Bullet *bl = GetFreeBullet();
              if (bl) {
                bl->Fire(-50.0f, (float)y, speed * 1.3f, 0.0f, false,
                         BulletType::BOSS);
                bl->SetImage(ResourceManager::GetInstance().GetImage(
                    L"Assets/Images/EnergyBullet/Energy (39).png"));
                bl->SetScale(1.8f);
                bl->SetRadius(25.0f);
                bl->SetNoDamage(false);
                bl->SetLifeTime(0.0f);
                if (inBox) {
                  bl->SetLifeTime(fabsf(r.x1 - (-50.0f)) / (speed * 1.3f));
                  Bullet *bl2 = GetFreeBullet();
                  if (bl2) {
                    bl2->Fire(r.x2 + 20.0f, (float)y, speed * 1.3f, 0.0f, false,
                              BulletType::BOSS);
                    bl2->SetImage(ResourceManager::GetInstance().GetImage(
                        L"Assets/Images/EnergyBullet/Energy (39).png"));
                    bl2->SetScale(1.8f);
                    bl2->SetRadius(25.0f);
                    bl2->SetNoDamage(false);
                    bl2->SetLifeTime(0.0f);
                    bl2->SetSpawnDelay(fabsf((r.x2 + 20.0f) - (-50.0f)) /
                                       (speed * 1.3f));
                  }
                }
              }
              Bullet *br = GetFreeBullet();
              if (br) {
                br->Fire(m_width + 50.0f, (float)y, -speed * 1.3f, 0.0f, false,
                         BulletType::BOSS);
                br->SetImage(ResourceManager::GetInstance().GetImage(
                    L"Assets/Images/EnergyBullet/Energy (35).png"));
                br->SetScale(1.8f);
                br->SetRadius(25.0f);
                br->SetNoDamage(false);
                br->SetLifeTime(0.0f);
                if (inBox) {
                  br->SetLifeTime(fabsf((m_width + 50.0f) - r.x2) /
                                  (speed * 1.3f));
                  Bullet *br2 = GetFreeBullet();
                  if (br2) {
                    br2->Fire(r.x1 - 20.0f, (float)y, -speed * 1.3f, 0.0f,
                              false, BulletType::BOSS);
                    br2->SetImage(ResourceManager::GetInstance().GetImage(
                        L"Assets/Images/EnergyBullet/Energy (35).png"));
                    br2->SetScale(1.8f);
                    br2->SetRadius(25.0f);
                    br2->SetNoDamage(false);
                    br2->SetLifeTime(0.0f);
                    br2->SetSpawnDelay(
                        fabsf((m_width + 50.0f) - (r.x1 - 20.0f)) /
                        (speed * 1.3f));
                  }
                }
              }
            }
          }
        }
      }
    }
  } break;

  case 27: {
    // [?�턴 ?�설�? ?�페르노 ??(Hell-Whip Slam)
    float t = boss->GetPatternTimer();
    float cycle = 1.3f; // 채찍�?주기 (빠르�?
    float swingT = fmodf(t, cycle) / cycle;

    // 보스 ?�손(마법�? ?�치 계산
    for (int side = -1; side <= 1; side += 2) {
      Vector2 handPos = {bp.x + side * 140.0f, bp.y + 40.0f};

      // 1. ?�두르는 마법�??�출 (고정 ?�셋)
      Bullet *magicCircle = GetFreeBullet();
      if (magicCircle) {
        magicCircle->Fire(handPos.x, handPos.y, 0.0f, 0.0f, false,
                          BulletType::BOSS);
        magicCircle->SetImage(ResourceManager::GetInstance().GetImage(
            L"Assets/Images/EnergyBullet/Energy (42).png"));
        magicCircle->SetNoDamage(true);
        magicCircle->SetScale(1.8f);
        magicCircle->SetLifeTime(0.08f);
        magicCircle->SetAlpha(0.6f);
      }

      // 2. ?�슬 채찍 ?�윙 로직
      // 각도 결정: 좌우 번갈?��?�??�게 ?�두�?(?�선 그리�?
      float startAngle = (side == -1) ? 220.0f : -40.0f;
      float endAngle = (side == -1) ? -40.0f : 220.0f;
      float currentDeg = startAngle + (endAngle - startAngle) * swingT;
      float rad = currentDeg * 3.14159f / 180.0f;

      int chainSegments = 18;
      float chainLength = 950.0f; // ?�면 ?�까지 ?�는 �??�슬

      for (int i = 1; i <= chainSegments; ++i) {
        float dist = (float)i * (chainLength / (float)chainSegments);
        // ?�심?�에 ?�해 ?�간 ?�는 곡선 ?�출
        float curve =
            sinf(swingT * 3.14159f) * 60.0f * ((float)i / chainSegments);
        Vector2 chainPos = {handPos.x + cosf(rad) * dist - sinf(rad) * curve,
                            handPos.y + sinf(rad) * dist + cosf(rad) * curve};

        // ?�슬 몸체 ?�환 ?�성
        Bullet *b = GetFreeBullet();
        if (b) {
          b->Fire(chainPos.x, chainPos.y, 0.0f, 0.0f, false, BulletType::BOSS);
          int imgId = (i % 2 == 0) ? 35 : 39;
          b->SetImage(ResourceManager::GetInstance().GetImage(
              L"Assets/Images/EnergyBullet/Energy (" +
              std::to_wstring(imgId) + L").png"));
          b->SetScale(1.3f);
          b->SetLifeTime(0.35f); // ?�상???�기�?빠르�??�멸
          b->SetAlpha(0.8f);
        }

        // 3. 바닥 ?��????�편 발생 (?�슬 ?��?분이 ?�면 ?�래쪽일 ??
        if (i == chainSegments && chainPos.y > m_height - 150.0f &&
            swingT > 0.4f && swingT < 0.6f) {
          // 강력???��??�편 (Explosion ?�???�환 ?��?)
          for (int p = 0; p < 3; ++p) {
            Bullet *fragment = GetFreeBullet();
            if (fragment) {
              float fDeg = (float)(rand() % 60 + 240); // ?�쪽?�로 ?�?�오�?
              float fRad = fDeg * 3.14159f / 180.0f;
              float fSpd = 300.0f + (rand() % 200);
              fragment->Fire(chainPos.x, chainPos.y, cosf(fRad) * fSpd,
                             sinf(fRad) * fSpd, false, BulletType::BOSS);
              fragment->SetImage(ResourceManager::GetInstance().GetImage(
                  L"Assets/Images/EnergyBullet/Energy (35).png"));
              fragment->SetScale(0.7f);
            }
          }
        }
      }
    }
  } break;
  case 30: { // Supernova: Collapse and Expansion
    if (boss->GetBossId() != 1)
      return;
    Vector2 bp = boss->GetPosition();
    float t = boss->GetPatternTimer();
    float mult = (m_stage == 3 ? boss->GetBulletSpeedMultiplier() : 1.0f);

    boss->AddPatternAngle1(18.0f);
    float angleBase = boss->GetPatternAngle1();

    // Left Sub-Boss (Pink/Purple Spiral) - Upgraded to 5-way
    if (boss->GetLeftHP() > 0) {
      float angle = angleBase * (3.14159f / 180.0f);
      for (int i = 0; i < 5; ++i) {
        float a = angle + i * (3.14159f * 2.0f / 5.0f);
        Bullet *b = GetFreeBullet();
        if (b) {
          float speed = 220.0f * mult;
          b->Fire(bp.x - 220.0f, bp.y, cosf(a) * speed, sinf(a) * speed, false,
                  BulletType::BOSS);
          int starId = 1 + (rand() % 6);
          b->SetImage(ResourceManager::GetInstance().GetImage(
              L"Assets/Images/StarBullet/Star (" + std::to_wstring(starId) +
              L").png"));
          b->SetScale(1.0f);
          b->SetRadius(12.0f);
          b->SetRotationRate(360.0f);
        }
      }
    }

    // Right Sub-Boss (Cyan/Blue Spiral, Reverse) - Upgraded to 5-way
    if (boss->GetRightHP() > 0) {
      float angle = -angleBase * (3.14159f / 180.0f);
      for (int i = 0; i < 5; ++i) {
        float a = angle + i * (3.14159f * 2.0f / 5.0f);
        Bullet *b = GetFreeBullet();
        if (b) {
          float speed = 220.0f * mult;
          b->Fire(bp.x + 220.0f, bp.y, cosf(a) * speed, sinf(a) * speed, false,
                  BulletType::BOSS);
          int starId = 1 + (rand() % 6);
          b->SetImage(ResourceManager::GetInstance().GetImage(
              L"Assets/Images/StarBullet/Star (" + std::to_wstring(starId) +
              L").png"));
          b->SetScale(1.0f);
          b->SetRadius(12.0f);
          b->SetRotationRate(-360.0f);
        }
      }
    }

    // Main Boss (Slow Tracking Energy Sphere)
    boss->AddPatternTimer1(deltaTime);
    if (boss->GetPatternTimer1() > 0.6f) {
      boss->SetPatternTimer1(0.0f);
      Bullet *b = GetFreeBullet();
      if (b) {
        Vector2 pp = m_player.GetPosition();
        float dx = pp.x - bp.x, dy = pp.y - bp.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > 0.1f) {
          float vx = (dx / dist) * 100.0f * mult,
                vy = (dy / dist) * 100.0f * mult;
          b->Fire(bp.x, bp.y + 50.0f, vx, vy, false, BulletType::BOSS);
          int ptrId = 1 + (rand() % 18);
          b->SetImage(ResourceManager::GetInstance().GetImage(
              L"Assets/Images/StarBullet/Pointer (" +
              std::to_wstring(ptrId) + L").png"));
          b->SetScale(2.5f);
          b->SetRadius(45.0f);
          b->SetRotationRate(180.0f);
        }
      }
    }
  } break;

  case 31: { // Synchronized Meteors
    if (boss->GetBossId() != 1)
      return;
    Vector2 bp = boss->GetPosition();
    float t = boss->GetPatternTimer();
    float mult = (m_stage == 3 ? boss->GetBulletSpeedMultiplier() : 1.0f);
    float wallSpeed = 450.0f * mult; // Slightly lowered for simultaneous firing
    float wallInterval = 0.35f;      // Shortened interval for denser walls

    boss->AddPatternTimer1(deltaTime);
    boss->AddPatternTimer2(deltaTime);

    // Right Sub-Boss starts together with Left (Removed staggered offset)
    if (t < 0.1f) {
      boss->SetPatternTimer1(0.0f);
      boss->SetPatternTimer2(0.0f);
    }

    // Combined Meteor Logic (Synchronized Gaps)
    if (boss->GetPatternTimer1() > wallInterval) {
      boss->SetPatternTimer1(0.0f);
      boss->SetPatternTimer2(0.0f); // Keep both synced

      // Generate 4 common gaps (using range 0-19 to fit both dimensions)
      int g[4];
      for (int i = 0; i < 4; ++i)
        g[i] = rand() % 19;

      // Left Sub-Boss: Vertical Meteors (X-axis gaps)
      if (boss->GetLeftHP() > 0) {
        for (int i = 0; i < 23; ++i) {
          bool isGap = false;
          for (int j = 0; j < 4; ++j)
            if (i == g[j] || i == g[j] + 1)
              isGap = true;

          if (!isGap) {
            Bullet *b = GetFreeBullet();
            if (b) {
              float x = (float)i * 45.0f;
              b->Fire(x, bp.y, 0.0f, wallSpeed, false, BulletType::BOSS);
              int sid = 7 + (rand() % 6);
              b->SetImage(ResourceManager::GetInstance().GetImage(
                  L"Assets/Images/StarBullet/Star (" + std::to_wstring(sid) +
                  L").png"));
              b->SetScale(1.5f);
              b->SetRadius(19.0f);
            }
          }
        }
      }

      // Right Sub-Boss: Horizontal Meteors (Y-axis gaps)
      if (boss->GetRightHP() > 0) {
        for (int i = 0; i < 20; ++i) {
          bool isGap = false;
          for (int j = 0; j < 4; ++j)
            if (i == g[j] || i == g[j] + 1)
              isGap = true;

          if (!isGap) {
            Bullet *b = GetFreeBullet();
            if (b) {
              float y = bp.y + (float)i * 45.0f;
              b->Fire((float)m_width + 50.0f, y, -wallSpeed, 0.0f, false,
                      BulletType::BOSS);
              int sid = 7 + (rand() % 6);
              b->SetImage(ResourceManager::GetInstance().GetImage(
                  L"Assets/Images/StarBullet/Star (" + std::to_wstring(sid) +
                  L").png"));
              b->SetScale(1.5f);
              b->SetRadius(19.0f);
            }
          }
        }
      }
    }
  } break;

  case 32: { // Guardian's Pentagram
    if (boss->GetBossId() != 1)
      return;
    Vector2 bp = boss->GetPosition();
    float t = boss->GetPatternTimer();
    float mult = (m_stage == 3 ? boss->GetBulletSpeedMultiplier() : 1.0f);
    float distR = 400.0f;

    if (t < 0.1f) {
      boss->SetPatternTimer1(0.8f);  // barrierTimer
      boss->SetPatternTimer2(0.0f);  // sweepTimer
      boss->SetPatternAngle1(90.0f); // sweepAngle
      boss->SetPatternBool1(true);   // sweepRight
    }

    boss->AddPatternTimer1(deltaTime);
    if (boss->GetPatternTimer1() >= 0.95f) {
      boss->SetPatternTimer1(0.0f);
      auto getV = [&](int i) {
        float a = (72.0f * (float)i - 90.0f) * (3.14159f / 180.0f);
        return Vector2(bp.x + cosf(a) * distR, bp.y + sinf(a) * distR);
      };
      bool leftAlive = (boss->GetLeftHP() > 0),
           rightAlive = (boss->GetRightHP() > 0);
      struct Edge {
        int v1, v2;
        bool active;
      };
      Edge edgeArr[] = {{0, 2, leftAlive},
                        {2, 4, leftAlive},
                        {4, 1, leftAlive && rightAlive},
                        {1, 3, rightAlive},
                        {3, 0, rightAlive}};
      std::vector<Edge> edges(edgeArr, edgeArr + 5);
      float convSpeed = (160.0f + (t * 12.0f)) * mult;
      if (!leftAlive || !rightAlive)
        convSpeed *= 0.7f;
      for (auto &edge : edges) {
        if (!edge.active)
          continue;
        Vector2 p1 = getV(edge.v1), p2 = getV(edge.v2);
        for (float f = 0.0f; f <= 1.0f; f += 0.14f) {
          float sx = p1.x + (p2.x - p1.x) * f, sy = p1.y + (p2.y - p1.y) * f;
          Bullet *b = GetFreeBullet();
          if (b) {
            float dx = bp.x - sx, dy = bp.y - sy;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist > 1.0f) {
              b->Fire(sx, sy, (dx / dist) * convSpeed, (dy / dist) * convSpeed,
                      false, BulletType::BOSS);
              b->SetImage(ResourceManager::GetInstance().GetImage(
                  L"Assets/Images/StarBullet/Star (7).png"));
              b->SetScale(1.4f);
              b->SetRadius(18.0f);
            }
          }
        }
      }
    }

    boss->AddPatternTimer2(deltaTime);
    if (boss->GetPatternTimer2() > 0.045f) { // 공격 주기 ?�향 (0.06 -> 0.045)
      boss->SetPatternTimer2(0.0f);
      Bullet *b = GetFreeBullet();
      if (b) {
        float rad = boss->GetPatternAngle1() * (3.14159f / 180.0f);
        float speed = 320.0f * mult;
        b->Fire(bp.x, bp.y + 50.0f, cosf(rad) * speed, sinf(rad) * speed, false,
                BulletType::BOSS);
        b->SetImage(ResourceManager::GetInstance().GetImage(
            L"Assets/Images/StarBullet/Pointer (12).png"));
        b->SetScale(0.8f);
        b->SetRadius(10.0f);
      }
      boss->AddPatternAngle1(boss->GetPatternBool1() ? 15.0f : -15.0f);
      if (boss->GetPatternAngle1() > 165.0f)
        boss->SetPatternBool1(false);
      else if (boss->GetPatternAngle1() < 15.0f)
        boss->SetPatternBool1(true);
    }
  } break;

  case 33: { // Chained Meteor Explosion
    if (boss->GetBossId() != 1)
      return;
    Vector2 bp = boss->GetPosition();
    float startX = (float)(rand() % (m_width - 150)) + 75.0f;

    Bullet *b = GetFreeBullet();
    if (b) {
      b->Fire(startX, bp.y, 0.0f, 150.0f, false, BulletType::BOSS);
      int imgId = 2 + (rand() % 9);
      b->SetImage(ResourceManager::GetInstance().GetImage(
          L"Assets/Images/EnergyBullet/Energy (" + std::to_wstring(imgId) +
          L").png"));
      b->SetScale(2.5f);
      b->SetRadius(40.0f);
      b->SetHP(10);
      b->SetRotationRate(90.0f);
    }
  } break;

  case 34: { // Supernova: Collapse & Expansion
    Vector2 bp = boss->GetPosition();
    float t = boss->GetPatternTimer();
    if (t < 0.1f)
      m_supernovaScale = 1.0f;

    if (t < 4.0f) {
      // Collapse Phase
      for (int side = 0; side < 2; ++side) {
        Bullet *b = GetFreeBullet();
        if (b) {
          float startX = (side == 0) ? -20.0f : (float)m_width + 20.0f;
          float startY = (float)(rand() % m_height);
          float dx = bp.x - startX, dy = bp.y - startY;
          float dist = sqrtf(dx * dx + dy * dy);
          if (dist > 0.1f) {
            float speed = 400.0f + (float)(rand() % 200);
            b->Fire(startX, startY, (dx / dist) * speed, (dy / dist) * speed,
                    false, BulletType::BOSS, 340);
            int starId = (rand() % 18) + 1;
            b->SetImage(ResourceManager::GetInstance().GetImage(
                L"Assets/Images/StarBullet/Star (" +
                std::to_wstring(starId) + L").png"));
            b->SetScale(0.8f);
            b->SetRadius(10.0f);
            b->SetRotationRate(360.0f);
          }
        }
      }

      boss->AddPatternTimer1(deltaTime);
      if (boss->GetPatternTimer1() > 0.05f) {
        boss->SetPatternTimer1(0.0f);
        Bullet *core = GetFreeBullet();
        if (core) {
          core->Fire(bp.x, bp.y, 0.0f, 0.0f, false, BulletType::BOSS, 341);
          core->SetImage(ResourceManager::GetInstance().GetImage(
              L"Assets/Images/EnergyBullet/Energy (44).png"));
          core->SetScale(m_supernovaScale);
          core->SetNoDamage(true);
          core->SetLifeTime(0.06f);
        }
      }
    } else if (t >= 4.0f && t < 4.15f) {
      // Expansion Phase
      m_explosions.push_back(std::make_unique<Explosion>(bp.x, bp.y, 400.0f));
      for (int layer = 0; layer < 5; ++layer) {
        int count = 40 + (layer * 10);
        float speed = 180.0f + (float)layer * 80.0f;
        for (int i = 0; i < count; ++i) {
          float angle =
              ((float)i * (360.0f / (float)count)) * (3.14159f / 180.0f);
          Bullet *b = GetFreeBullet();
          if (b) {
            b->Fire(bp.x, bp.y, cosf(angle) * speed, sinf(angle) * speed, false,
                    BulletType::BOSS);
            int starId = (rand() % 18) + 1;
            b->SetImage(ResourceManager::GetInstance().GetImage(
                L"Assets/Images/StarBullet/Star (" +
                std::to_wstring(starId) + L").png"));
            b->SetScale(1.2f);
            b->SetRadius(15.0f);
            b->SetRotationRate((float)(rand() % 720 - 360));
          }
        }
      }
    }
  } break;

  case 35: { // Orbital Resonance + Branding
    Vector2 bp = boss->GetPosition();
    float t = boss->GetPatternTimer();

    struct Ring {
      float radius;
      float speed;
      int countPerSec;
      int minStar;
      int maxStar;
    };
    Ring rings[] = {{200.0f, 52.0f, 65, 1, 6},
                    {380.0f, -42.0f, 55, 7, 12},
                    {560.0f, 28.0f, 45, 13, 18}};

    float lastPTimer = boss->GetPatternTimer4();
    float accum[3] = {boss->GetPatternAngle2(), boss->GetPatternAngle3(),
                      boss->GetPatternAngle4()};
    bool isInitialFrame = false;
    if (t < 0.1f && lastPTimer > 2.0f) {
      lastPTimer = 0.0f;
      accum[0] = accum[1] = accum[2] = 0.0f;
      isInitialFrame = true;
    } else if (t < 0.1f && lastPTimer == 0.0f) {
      isInitialFrame = true;
    }
    float dt = max(0.0f, t - lastPTimer);
    boss->SetPatternTimer4(t);

    for (int rIdx = 0; rIdx < 3; ++rIdx) {
      const auto &ring = rings[rIdx];
      if (rIdx == 0) {
        if (isInitialFrame) {
          const int innerCount = 32;
          for (int i = 0; i < innerCount; ++i) {
            float deg = (float)i * (360.0f / (float)innerCount);
            Bullet *b = GetFreeBullet();
            if (b) {
              float rad = deg * 3.14159f / 180.0f;
              b->Fire(bp.x + cosf(rad) * ring.radius,
                      bp.y + sinf(rad) * ring.radius, 0.0f, 0.0f, false,
                      BulletType::BOSS);
              float relT = 1.5f + (float)i * 0.42f;
              b->SetOrbit(true, boss->GetBossId(), ring.radius, ring.speed, deg,
                          relT, Vector2{0.0f, 0.0f});
              int sid =
                  ring.minStar + (rand() % (ring.maxStar - ring.minStar + 1));
              b->SetImage(ResourceManager::GetInstance().GetImage(
                  L"Assets/Images/StarBullet/Star (" + std::to_wstring(sid) +
                  L").png"));
              b->SetScale(1.0f);
              b->SetRadius(12.0f);
            }
          }
          accum[rIdx] = 0.0f;
          boss->SetPatternAngle2(0.0f);
        }
        continue;
      }

      accum[rIdx] += (float)ring.countPerSec * dt; // ?�래 밀?�로 ?�복
      while (accum[rIdx] >= 1.0f) {
        accum[rIdx] -= 1.0f;
        float baseA = (rIdx == 1) ? 180.0f : 0.0f;
        float rad = baseA * 3.14159f / 180.0f;
        Vector2 entry = {bp.x + cosf(rad) * ring.radius,
                         bp.y + sinf(rad) * ring.radius};
        float virtA =
            (t * ring.speed) + (t * (float)(rIdx % 2 == 0 ? 1 : -1) * 35.0f);
        float chkA = fmodf(virtA, 360.0f);
        if (chkA < 0)
          chkA += 360.0f;

        bool inGap = false;
        for (int g = 0; g < 4; ++g) {
          float diff = fabsf(chkA - (float)g * 90.0f);
          if (diff > 180.0f)
            diff = 360.0f - diff;
          if (diff < 22.0f) {
            inGap = true;
            break;
          }
        }

        if (!inGap) {
          Bullet *b = GetFreeBullet();
          if (b) {
            b->Fire(entry.x, entry.y, 0.0f, 0.0f, false, BulletType::BOSS);
            b->SetOrbit(true, boss->GetBossId(), ring.radius, ring.speed, baseA,
                        99.03f, Vector2{0.0f, 0.0f});
            int sid =
                ring.minStar + (rand() % (ring.maxStar - ring.minStar + 1));
            b->SetImage(ResourceManager::GetInstance().GetImage(
                L"Assets/Images/StarBullet/Star (" + std::to_wstring(sid) +
                L").png"));
            b->SetScale(1.0f);
            b->SetRadius(12.0f);
          }
        }
      }
      if (rIdx == 0)
        boss->SetPatternAngle2(accum[0]);
      else if (rIdx == 1)
        boss->SetPatternAngle3(accum[1]);
      else if (rIdx == 2)
        boss->SetPatternAngle4(accum[2]);
    }

    float targets[] = {3.0f, 7.0f, 11.0f, 15.0f, 19.0f};
    float lastBrand = boss->GetPatternTimer3();
    if (t < 0.1f)
      lastBrand = -1.0f;
    for (float trg : targets) {
      if (t >= trg && lastBrand < trg) {
        lastBrand = t;
        boss->SetPatternTimer3(t);
        Vector2 pp = m_player.GetPosition();
        Bullet *b = GetFreeBullet();
        if (b) {
          b->Fire(pp.x, pp.y, 0.0f, 0.0f, false, BulletType::BOSS, 351);
          b->SetImage(ResourceManager::GetInstance().GetImage(
              L"Assets/Images/EnergyBullet/Energy (45).png"));
          b->SetScale(2.5f);
          b->SetNoDamage(true);
          b->SetLifeTime(10.0f);
        }
      }
    }
  } break;
  case 36: {
    if (boss->GetBossId() != 1)
      return;
    if (boss->GetStage() == 3 &&
        (boss->GetLeftHP() > 0 || boss->GetRightHP() > 0))
      return;

    float patternT = boss->GetPatternTimer();
    float cycleDuration = 12.0f;
    float cycleT = fmodf(patternT, cycleDuration);
    Vector2 bp = boss->GetPosition();

    if (cycleT < 0.2f) {
      float lastSpawnT = boss->GetPatternTimer1();
      if (cycleT < 0.1f) {
        lastSpawnT = -1.0f;
        boss->SetPatternTimer1(-1.0f);
      }
      if (patternT - lastSpawnT >= 1.0f) {
        lastSpawnT = patternT;
        boss->SetPatternTimer1(patternT);
        float speed = 600.0f;
        float low = 1080.0f * 0.22f;
        float high = 1080.0f * 0.78f;
        float margin = 60.0f;

        struct Tri {
          Vector2 p1, p2, p3;
        };
        Tri tris[4] = {{Vector2{low, -margin}, Vector2{high, -margin}, bp},
                       {Vector2{low, 1080.0f + margin},
                        Vector2{high, 1080.0f + margin}, bp},
                       {Vector2{-margin, low}, Vector2{-margin, high}, bp},
                       {Vector2{1080.0f + margin, low},
                        Vector2{1080.0f + margin, high}, bp}};

        for (int s = 0; s < 4; ++s) {
          for (int i = 0; i < 100; ++i) {
            float r1 = (float)rand() / (float)RAND_MAX;
            float r2 = (float)rand() / (float)RAND_MAX;
            if (r1 + r2 > 1.0f) {
              r1 = 1.0f - r1;
              r2 = 1.0f - r2;
            }
            Vector2 spawnPos = {
                tris[s].p1.x + r1 * (tris[s].p2.x - tris[s].p1.x) +
                    r2 * (tris[s].p3.x - tris[s].p1.x),
                tris[s].p1.y + r1 * (tris[s].p2.y - tris[s].p1.y) +
                    r2 * (tris[s].p3.y - tris[s].p1.y)};
            float dx = bp.x - spawnPos.x, dy = bp.y - spawnPos.y;
            float d = sqrtf(dx * dx + dy * dy);
            float vx = (dx / (d > 0.1f ? d : 1.0f)) * speed;
            float vy = (dy / (d > 0.1f ? d : 1.0f)) * speed;

            Bullet *b = GetFreeBullet();
            if (b) {
              b->Fire(spawnPos.x, spawnPos.y, vx, vy, false, BulletType::BOSS,
                      360);
              int sid = 15 + (rand() % 4);
              b->SetImage(ResourceManager::GetInstance().GetImage(
                  L"Assets/Images/StarBullet/Star (" + std::to_wstring(sid) +
                  L").png"));
              b->SetScale(1.05f);
              b->SetRadius(12.0f);
              b->SetPhase(0);
              b->SetLifeTime(15.0f);
              int gIdx = s * 100 + i;
              float uA = (float)gIdx * (360.0f / 400.0f);
              b->SetOrbitIndex((float)gIdx);
              b->SetOrbit(true, boss->GetBossId(), 60.0f + (float)(rand() % 45),
                          180.0f, uA, 99.0f, Vector2{0.0f, 0.0f});
            }
          }
        }
      }
    }

    if (cycleT >= 3.2f && cycleT < 6.5f) {
      for (auto *b : m_activeBullets) {
        if (b->GetVariant() == 360 && b->GetPhase() == 0)
          b->SetPhase(1);
      }
    }

    bool shieldSpawned = boss->GetPatternBool2();
    float lastCycleT = boss->GetPatternTimer5();
    if (cycleT < lastCycleT) {
      shieldSpawned = false;
      boss->SetPatternBool2(false);
    }
    boss->SetPatternTimer5(cycleT);

    if (cycleT >= 3.5f && cycleT < 6.0f && !shieldSpawned) {
      m_itemsToAdd.push_back(
          std::make_unique<Item>(512.0f, 768.0f, ItemType::SHIELD));
      boss->SetPatternBool2(true);
    }

    if (cycleT >= 6.0f && cycleT < 6.5f) {
      for (auto &itm : m_items) {
        if (itm->IsActive() && itm->GetType() == ItemType::SHIELD)
          itm->SetActive(false);
      }
    }

    if (cycleT >= 6.5f) {
      for (auto *b : m_activeBullets) {
        if (b->GetVariant() == 360 && b->GetPhase() == 1)
          b->SetPhase(2);
      }
    }
  } break;

  case 37: { // Stigmatic Milky Way
    if (boss->GetBossId() != 1)
      return;
    if (boss->GetStage() == 3 &&
        (boss->GetLeftHP() > 0 || boss->GetRightHP() > 0))
      return;

    float t = boss->GetPatternTimer();
    float centerX = (float)m_width / 2.0f + sinf(t * 1.5f) * 360.0f;
    float safeHalfW = 135.0f;
    float speed = 400.0f;

    for (float x = 0.0f; x <= (float)m_width; x += 35.0f) {
      if (fabsf(x - centerX) < safeHalfW)
        continue;

      Bullet *b = GetFreeBullet();
      if (b) {
        b->Fire(x, -30.0f, 0.0f, speed, false, BulletType::BOSS);
        int sid = 7 + (rand() % 6);
        b->SetImage(ResourceManager::GetInstance().GetImage(
            L"Assets/Images/StarBullet/Star (" + std::to_wstring(sid) +
            L").png"));
        b->SetScale(1.0f);
        b->SetRadius(12.0f);
        b->SetRotationRate((float)(rand() % 360 - 180));
      }
    }
  } break;

  case 38: { // Final Spell: Astral End
    float t = boss->GetPatternTimer();
    Vector2 pp = m_player.GetPosition();
    float waveT = fmodf(t, 3.0f);
    int waveIdx = (int)(t / 3.0f);

    // 1. Spawning Phase
    if (waveIdx < 8 && waveT < 1.8f) {
      for (int i = 0; i < 3; ++i) {
        float rx = (float)(rand() % m_width);
        float ry = (float)(rand() % 512);
        Bullet *b = GetFreeBullet();
        if (b) {
          b->Fire(rx, ry, 0.0f, 0.0f, false, BulletType::BOSS, 380);
          int sid = 15 + (rand() % 4);
          b->SetImage(ResourceManager::GetInstance().GetImage(
              L"Assets/Images/StarBullet/Star (" + std::to_wstring(sid) +
              L").png"));
          b->SetScale(0.85f);
          b->SetRadius(8.0f);
          b->SetAlpha(0.0f);
          b->SetPhase(0);
          b->SetLifeTime(60.0f);
        }
      }
    }

    for (auto *b : m_activeBullets) {
      if (b->GetVariant() == 380) {
        if (b->GetPhase() == 0) {
          float a = b->GetAlpha();
          if (a < 1.0f)
            b->SetAlpha((a + deltaTime * 3.0f > 1.0f) ? 1.0f
                                                      : a + deltaTime * 3.0f);
        }
      }
    }

    // 2. Release Phase
    if (waveT >= 2.0f && waveT < 2.06f) {
      float speed = 680.0f; // ?�속 ?�향 (440 -> 680)
      for (auto *b : m_activeBullets) {
        if (b->GetVariant() == 380 && b->GetPhase() == 0) {
          Vector2 bpos = b->GetPosition();
          float dx = pp.x - bpos.x, dy = pp.y - bpos.y;
          float dist = sqrtf(dx * dx + dy * dy);
          if (dist < 0.1f)
            dist = 1.0f;
          b->SetVelocity((dx / dist) * speed, (dy / dist) * speed);
          b->SetAlpha(1.0f);
          b->SetPhase(1);
          b->SetRotationRate((float)(rand() % 400 - 200));
        }
      }
    }
  } break;

  case 39: {
    // [?�규 가???�공 ?�턴] ?�계???�공 (Star-System Pincer)
    if (boss->GetBossId() != 1)
      return;
    Vector2 bp = boss->GetPosition();
    float t = boss->GetPatternTimer();
    float mult = (m_stage == 3 ? boss->GetBulletSpeedMultiplier() : 1.0f);

    bool leftAlive = (boss->GetLeftHP() > 0);
    bool rightAlive = (boss->GetRightHP() > 0);
    if (!leftAlive && !rightAlive)
      return;

    // Phase 1: 바이?�리 ?�로??(0.0s ~ 6.0s) - X??교차 ?�격
    if (t < 6.0f) {
      float progress = t / 6.0f;
      Vector2 startL = {bp.x - 220.0f, bp.y + 100.0f};
      Vector2 startR = {bp.x + 220.0f, bp.y + 100.0f};
      Vector2 targetL = {(float)m_width * 0.92f, (float)m_height * 0.85f};
      Vector2 targetR = {(float)m_width * 0.08f, (float)m_height * 0.85f};

      Vector2 curL = {startL.x + (targetL.x - startL.x) * progress,
                      startL.y + (targetL.y - startL.y) * progress};
      Vector2 curR = {startR.x + (targetR.x - startR.x) * progress,
                      startR.y + (targetR.y - startR.y) * progress};

      if (fmodf(t, 0.08f) < deltaTime) {
        if (leftAlive) {
          Bullet *b = GetFreeBullet();
          if (b) {
            b->Fire(curL.x, curL.y, 0.0f, 0.0f, false, BulletType::BOSS);
            b->SetImage(ResourceManager::GetInstance().GetImage(
                L"Assets/Images/StarBullet/Star (15).png"));
            b->SetScale(1.3f);
            b->SetLifeTime(4.0f);
          }
        }
        if (rightAlive) {
          Bullet *b = GetFreeBullet();
          if (b) {
            b->Fire(curR.x, curR.y, 0.0f, 0.0f, false, BulletType::BOSS);
            b->SetImage(ResourceManager::GetInstance().GetImage(
                L"Assets/Images/StarBullet/Star (17).png"));
            b->SetScale(1.3f);
            b->SetLifeTime(4.0f);
          }
        }
      }
    }
    // Phase 2: ?��??�스???�이 (6.0s ~ 14.0s) - [?�이??밸런??버전]
    else {
      float pHeight = m_player.GetPosition().y;
      float curWingY = boss->GetPatternTimer2();
      if (curWingY < 1.0f)
        curWingY = bp.y + 100.0f;
      curWingY += (pHeight - curWingY) * 0.02f; // 추적 지???�용
      boss->SetPatternTimer2(curWingY);

      float cycleT = fmodf(t, 2.0f);
      bool isFiringTime = (cycleT < 1.25f); // 간헐???�격
      float safeMargin = 280.0f;

      if (isFiringTime && (fmodf(t, 0.12f) < deltaTime)) {
        if (leftAlive) {
          Bullet *bl = GetFreeBullet();
          if (bl) {
            bl->Fire(safeMargin - 150.0f, curWingY, 520.0f, 0.0f, false,
                     BulletType::BOSS);
            bl->SetImage(ResourceManager::GetInstance().GetImage(
                L"Assets/Images/StarBullet/Pointer (12).png"));
            bl->SetScale(0.7f);
            bl->SetAlpha(0.6f);
          }
        }
        if (rightAlive) {
          Bullet *br = GetFreeBullet();
          if (br) {
            br->Fire((float)m_width - safeMargin + 150.0f, curWingY, -520.0f,
                     0.0f, false, BulletType::BOSS);
            br->SetImage(ResourceManager::GetInstance().GetImage(
                L"Assets/Images/StarBullet/Pointer (12).png"));
            br->SetScale(0.7f);
            br->SetAlpha(0.6f);
          }
        }
      }
      // 본체 ?��? ?�격
      if (fmodf(t, 0.65f) < deltaTime)
        FireEnemyRadial(bp, 12, 260.0f * mult, BulletType::BOSS);
    }
  } break;
  }
}
void Game::FireEnemyRadial(Vector2 pos, int count, float speed,
                           BulletType type) {
  float step = 360.0f / (float)count;
  static int ebc = 0;
  for (int i = 0; i < count; ++i) {
    float a = (float)i * step * (3.14159f / 180.0f);
    Bullet *b = GetFreeBullet();
    if (b) {
      b->Fire(pos.x, pos.y, cosf(a) * speed, sinf(a) * speed, false, type,
              ebc++);
      if (type == BulletType::BOSS) {
        std::wstring path = L"";
        if (m_stage == 1) {
          int cId = (ebc % 18) + 1;
          path = L"Assets/Images/RealBullet/BossBullet (" +
                 std::to_wstring(cId) + L").png";
        } else if (m_stage == 2) {
          int cId = (ebc % 45) + 1;
          path = L"Assets/Images/EnergyBullet/Energy (" +
                 std::to_wstring(cId) + L").png";
        } else if (m_stage == 3) {
          int cId = (ebc % 18) + 1;
          path = L"Assets/Images/StarBullet/Star (" + std::to_wstring(cId) +
                 L").png";
        }

        if (!path.empty())
          b->SetImage(ResourceManager::GetInstance().GetImage(path));
        else
          b->SetImage(
              ResourceManager::GetInstance().GetBulletImage(ebc, m_stage == 2));
      } else
        b->SetImage(ResourceManager::GetInstance().GetEnemyBulletImage());
    }
  }
}

Bullet *Game::GetFreeBullet() {
  if (m_inactiveBullets.empty())
    return nullptr;
  Bullet *b = m_inactiveBullets.back();
  m_inactiveBullets.pop_back();
  m_activeBullets.push_back(b);
  return b;
}

std::wstring Game::GetBackgroundPath(int stage) {
  switch (stage) {
  case 1:
    return L"Assets/Images/BackGround/Stage1/Starfield 5 - 1024x1024.png";
  case 2:
    return L"Assets/Images/BackGround/Stage2/Blue Nebula 3 - 1024x1024.png";
  case 3:
    return L"Assets/Images/BackGround/Stage3/Green Nebula 1 - 1024x1024.png";
  default:
    return L"Assets/Images/BackGround/Stage1/Starfield 5 - 1024x1024.png";
  }
}

void Game::CheckCollisions() {
  // 1. POPULATE GRID (Always run this for Rendering Culling, regardless of
  // player state)
  m_grid->Clear();
  for (auto &e : m_enemies)
    if (e->IsActive())
      m_grid->Insert(e.get());
  for (auto b : m_activeBullets)
    if (b->IsActive())
      m_grid->Insert(b);
  for (auto &i : m_items)
    if (i->IsActive())
      m_grid->Insert(i.get());

  // 2. LOGICAL COLLISION CHECK (Skip if player is dead/respawning)
  if (m_isGameOver || m_player.IsRespawning())
    return;

  Vector2 pp = m_player.GetPosition();
  float pR = m_player.GetRadius();
  bool playerInvincible = m_player.IsInvincible();

  for (auto b : m_activeBullets) {
    if (!b->IsActive())
      continue;
    Vector2 bp = b->GetPosition();
    float bR = b->GetRadius();

    if (b->IsFriendly()) {
      static std::vector<Entity *> nearby;
      nearby.clear();
      m_grid->GetNearbyEntities(bp.x, bp.y, bR + 50.0f, nearby);

      // A. Check against Grid (Enemies, Bullets, Items)
      for (auto *entity : nearby) {
        EntityLayer layer = entity->GetLayer();

        // Player Bullet vs Enemy
        if (layer == EntityLayer::ENEMY) {
          Enemy *e = static_cast<Enemy *>(entity);
          if (!e->IsActive())
            continue;
          Vector2 ep = e->GetPosition();
          float d2 =
              (bp.x - ep.x) * (bp.x - ep.x) + (bp.y - ep.y) * (bp.y - ep.y);
          float rS = e->GetRadius() + bR;
          if (d2 < rS * rS) {
            b->SetActive(false);
            e->TakeDamage(1);
            m_explosions.push_back(
                std::make_unique<Explosion>(bp.x, bp.y, 65.0f));
            if (!e->IsActive()) {
              m_score += 300;
              m_killCount++;
              // ?�이???�률 100%�??�향
              {
                int r = rand() % 100;
                ItemType it = ItemType::POWERUP;
                if (r < 30)
                  it = ItemType::PLASMA;
                else if (r < 60)
                  it = ItemType::VULCAN;
                m_itemsToAdd.push_back(std::make_unique<Item>(ep.x, ep.y, it));
              }
            }
            break;
          }
        }
        // Player Bullet vs Destructible Enemy Bullet
        else if (layer == EntityLayer::BULLET) {
          Bullet *targetB = static_cast<Bullet *>(entity);
          if (!targetB->IsActive() || targetB->IsFriendly() ||
              !targetB->IsDestructible() || targetB == b)
            continue;
          Vector2 tp = targetB->GetPosition();
          float d2 =
              (bp.x - tp.x) * (bp.x - tp.x) + (bp.y - tp.y) * (bp.y - tp.y);
          float rS = bR + targetB->GetRadius();
          if (d2 < rS * rS) {
            b->SetActive(false);
            if (targetB->TakeDamage(m_player.GetDamage())) {
              targetB->SetActive(false);
              float mult =
                  (m_stage == 3 ? m_bosses[0]->GetBulletSpeedMultiplier()
                                : 1.0f);
              for (int i = 0; i < 24; ++i) {
                float angle = (i * 15.0f) * 3.14159f / 180.0f;
                Bullet *frag = GetFreeBullet();
                if (frag) {
                  frag->Fire(tp.x, tp.y, cosf(angle) * 250.0f * mult,
                             sinf(angle) * 250.0f * mult, false,
                             BulletType::BOSS);
                  frag->SetImage(ResourceManager::GetInstance().GetImage(
                      L"Assets/Images/StarBullet/Star (" +
                      std::to_wstring((rand() % 18) + 1) + L").png"));
                  frag->SetScale(0.5f);
                  frag->SetRadius(5.0f);
                }
              }
              m_explosions.push_back(
                  std::make_unique<Explosion>(tp.x, tp.y, 80.0f));
            }
            break;
          }
        }
      }

      // B. Check against Bosses directly (Bosses are large, not suitable for
      // small grid search)
      if (b->IsActive()) {
        for (auto &boss : m_bosses) {
          if (!boss->IsActive())
            continue;
          Vector2 bossPos = boss->GetPosition();
          bool isHit = false;
          if (boss->GetStage() == 3) {
            float dx_main = bp.x - bossPos.x;
            float dy_main = bp.y - bossPos.y;
            float distMain = sqrtf(dx_main * dx_main + dy_main * dy_main);
            if (distMain < 120.0f + bR) {
              isHit = true;
            } else {
              // Check wings (considering movement patterns)
              int cp = boss->GetCurrentPattern();
              float t = boss->GetPatternTimer();
              auto getWingPos = [&](int i) {
                if (cp == 32) {
                  float a = (72.0f * i - 90.0f) * (3.14159f / 180.0f);
                  Vector2 targetV(bossPos.x + cosf(a) * 400.0f,
                                  bossPos.y + sinf(a) * 400.0f);
                  Vector2 startPos = bossPos;
                  if (i == 2 || i == 4)
                    startPos.x -= 220.0f;
                  else
                    startPos.x += 220.0f;
                  float f = (std::min)(1.0f, t / 2.0f);
                  return Vector2{startPos.x + (targetV.x - startPos.x) * f,
                                 startPos.y + (targetV.y - startPos.y) * f};
                }
                return Vector2{(i == 2 || i == 4) ? bossPos.x - 220.0f
                                                  : bossPos.x + 220.0f,
                               bossPos.y};
              };

              // Left wing check
              Vector2 lpos = getWingPos(2);
              float dLX = bp.x - lpos.x, dLY = bp.y - lpos.y;
              float dL = sqrtf(dLX * dLX + dLY * dLY);
              // Right wing check
              Vector2 rpos = getWingPos(1);
              float dRX = bp.x - rpos.x, dRY = bp.y - rpos.y;
              float dR_wing = sqrtf(dRX * dRX + dRY * dRY);

              if (dL < 140.0f + bR || dR_wing < 140.0f + bR)
                isHit = true;
            }
          } else {
            float dx = bp.x - bossPos.x;
            float dy = bp.y - bossPos.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist < boss->GetRadius() + bR)
              isHit = true;
          }

          if (isHit) {
            int dmg = m_player.GetDamage();
            if (b->GetVariant() == 250) {
              dmg = (int)(boss->GetMaxHP() * 0.25f);
              if (dmg < 1)
                dmg = 1;
            }
            boss->TakeDamage(dmg, bp.x);
            b->SetActive(false);
            if (m_bossHitEffectTimer <= 0) {
              m_explosions.push_back(
                  std::make_unique<Explosion>(bp.x, bp.y, 135.0f));
              m_bossHitEffectTimer = 0.18f;
            }
            break;
          }
        }
      }
    } else if (!playerInvincible) {
      // Enemy/Boss Bullet vs Player
      if (b->IsNoDamage())
        continue;
      float d2 = (bp.x - pp.x) * (bp.x - pp.x) + (bp.y - pp.y) * (bp.y - pp.y);
      float rS = pR + bR;
      if (d2 < rS * rS) {
        b->SetActive(false);
        m_score = (std::max)(0, m_score - 1000);
        m_playerLife--;
        m_totalRespawns++;
        m_explosions.push_back(std::make_unique<Explosion>(pp.x, pp.y, 100.0f));
        if (m_playerLife > 0)
          m_player.StartRespawn();
      }
    }
  }
  if (!m_itemsToAdd.empty()) {
    for (auto &i : m_itemsToAdd)
      m_items.push_back(std::move(i));
    m_itemsToAdd.clear();
  }
  pp = m_player.GetPosition(); // Refresh player position for item check
  for (auto &item : m_items) {
    if (!item->IsActive())
      continue;
    Vector2 ip = item->GetPosition();
    float d2 = (pp.x - ip.x) * (pp.x - ip.x) + (pp.y - ip.y) * (pp.y - ip.y);
    float rS = m_player.GetRadius() + item->GetRadius();
    if (d2 < rS * rS) {
      item->SetActive(false);
      if (item->GetType() == ItemType::POWERUP) {
        m_player.LevelUp();
        m_statusMsg = L"POWER UP!";
        m_msgTimer = 1.5f;
      } else if (item->GetType() == ItemType::PLASMA) {
        m_player.EnablePlasma();
        m_statusMsg = L"PLASMA UP!";
        m_msgTimer = 1.5f;
      } else if (item->GetType() == ItemType::VULCAN) {
        m_player.EnableVulcan();
        m_statusMsg = L"VULCAN UP!";
        m_msgTimer = 1.5f;
      } else if (item->GetType() == ItemType::HOMING) {
        m_player.SetHoming(true);
        m_statusMsg = L"HOMING ENABLED!";
        m_msgTimer = 1.5f;
      } else if (item->GetType() == ItemType::SHIELD) {
        m_player.SetShield(true);
        m_player.SetInvincible(3.0f);
        m_statusMsg = L"SHIELD ENABLED (3s)!";
        m_msgTimer = 3.0f;
      }
    }
  }
}

void Game::Render() {
  if (!m_renderer)
    return;

  m_renderer->BeginRender(DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));

  if (m_state == GameState::START) {
    ID3D11ShaderResourceView *sImg = ResourceManager::GetInstance().GetImage(
        L"Assets/Images/BackGround/Start/StartImage.png");
    if (sImg) {
      m_renderer->DrawTexture(sImg, (float)m_width / 2.0f,
                              (float)m_height / 2.0f, (float)m_width,
                              (float)m_height);
    }
    m_renderer->DrawTextW(L"PRESS ENTER TO START", (float)m_width / 2.0f,
                          (float)m_height / 2.0f + 100, 30,
                          DirectX::XMFLOAT4(1, 1, 1, 1), true);
  } else {
    // --- Always Draw Background and Entities ---
    auto drawBG = [&](int stage, float alpha) {
      ID3D11ShaderResourceView *bgImg =
          ResourceManager::GetInstance().GetImage(GetBackgroundPath(stage));
      if (!bgImg)
        return;
      float bh = 1024.0f;
      float curY = fmodf(m_bgY, bh);
      m_renderer->DrawTexture(bgImg, (float)m_width / 2.0f, curY - bh / 2.0f,
                              (float)m_width, bh, 0.0f, alpha);
      m_renderer->DrawTexture(bgImg, (float)m_width / 2.0f, curY + bh / 2.0f,
                              (float)m_width, bh, 0.0f, alpha);
    };

    if (m_bgTransitionAlpha < 1.0f) {
      drawBG(m_prevStage, 1.0f);
      drawBG(m_stage, m_bgTransitionAlpha);
    } else {
      drawBG(m_stage, 1.0f);
    }

    for (auto b : m_activeBullets)
      b->Draw(m_renderer.get());
    for (auto &i : m_items)
      if (i->IsActive())
        i->Draw(m_renderer.get());
    for (auto &e : m_enemies)
      if (e->IsActive())
        e->Draw(m_renderer.get());
    for (auto &b : m_bosses)
      b->Draw(m_renderer.get());
    if (m_player.IsActive())
      m_player.Draw(m_renderer.get());
    for (auto &ex : m_explosions)
      ex->Draw(m_renderer.get());

    // Standard HUD
    m_renderer->DrawTextW(L"SCORE: " + std::to_wstring(m_score), 20, 20, 30,
                          DirectX::XMFLOAT4(1, 1, 1, 1));
    m_renderer->DrawTextW(L"LIFE: " + std::to_wstring(m_playerLife), 20, 60, 30,
                          DirectX::XMFLOAT4(1, 1, 1, 1));
    m_renderer->DrawTextW(L"FPS: " + std::to_wstring(m_currentFps),
                          (float)m_width - 120, 20, 25,
                          DirectX::XMFLOAT4(0, 1, 0, 1));

    if (m_isBossStage && !m_bosses.empty()) {
      float totalHP = 0.0f, totalMaxHP = 0.0f;
      for (auto &boss : m_bosses)
        if (boss->IsActive()) {
          totalHP += (float)boss->GetHP();
          totalMaxHP += (float)boss->GetMaxHP();
        }
      if (totalMaxHP > 0.0f) {
        float hpPercent = totalHP / totalMaxHP;
        float barW = (float)m_width * 0.7f;
        float barX = (float)m_width / 2.0f - barW / 2.0f;
        m_renderer->DrawRect(barX, 10.0f, barW, 20.0f,
                             DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f));
        m_renderer->DrawRect(barX, 10.0f, barW * hpPercent, 20.0f,
                             DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f));
      }
    }

    // --- State Specific Overlays ---
    if (m_state == GameState::GAMEOVER) {
      m_renderer->DrawRect(0, 0, (float)m_width, (float)m_height,
                           DirectX::XMFLOAT4(0, 0, 0, 0.5f)); // Dark Overlay
      m_renderer->DrawTextW(L"GAME OVER", (float)m_width / 2.0f,
                            (float)m_height / 2.0f - 50, 60,
                            DirectX::XMFLOAT4(1, 0, 0, 1), true);
      m_renderer->DrawTextW(L"Press [1] to Restart   |   [2] to Continue",
                            (float)m_width / 2.0f, (float)m_height / 2.0f + 50,
                            30, DirectX::XMFLOAT4(1, 1, 1, 1), true);
    } else if (m_state == GameState::GAMECLEAR) {
      m_renderer->DrawRect(
          0, 0, (float)m_width, (float)m_height,
          DirectX::XMFLOAT4(0, 0, 0, 0.7f)); // Scoreboard Background
      m_renderer->DrawTextW(L"-- MISSION COMPLETE --", (float)m_width / 2.0f,
                            (float)m_height / 2.0f - 150, 50,
                            DirectX::XMFLOAT4(1, 1, 0, 1), true);
      m_renderer->DrawTextW(L"FINAL SCORE: " + std::to_wstring(m_score),
                            (float)m_width / 2.0f, (float)m_height / 2.0f, 40,
                            DirectX::XMFLOAT4(1, 1, 1, 1), true);
      m_renderer->DrawTextW(L"Restarting in " +
                                std::to_wstring((int)ceil(m_msgTimer)) + L"...",
                            (float)m_width / 2.0f, (float)m_height / 2.0f + 150,
                            30, DirectX::XMFLOAT4(1, 1, 1, 1), true);
    }

    // --- Status Message Overlay (Center Screen) ---
    if (m_msgTimer > 0) {
      DirectX::XMFLOAT4 msgColor(1, 1, 1, 1);
      int fontSize = 45;

      if (m_statusMsg.find(L"WARNING") != std::wstring::npos) {
        msgColor = DirectX::XMFLOAT4(1, 0, 0, 1);
        fontSize = 55;
        if (fmodf(m_msgTimer, 0.4f) < 0.2f)
          msgColor.w = 0.3f;
      }

      m_renderer->DrawTextW(m_statusMsg, (float)m_width / 2.0f,
                            (float)m_height / 2.0f - 100, fontSize, msgColor,
                            true);
    }
  }

  m_renderer->EndRender();
}

void Game::Cleanup() {
  m_bulletPool.clear();
  m_activeBullets.clear();
  m_inactiveBullets.clear();
  m_enemies.clear();
  m_explosions.clear();
  m_items.clear();
  m_enemiesToAdd.clear();
  m_itemsToAdd.clear();
  m_bosses.clear();
  ResourceManager::GetInstance().Cleanup();
  if (m_renderer)
    m_renderer->Cleanup();
}
void Game::FireSubBossDeathPattern(Vector2 pos) {
  int count = 160;
  float step = 360.0f / count;
  for (int i = 0; i < count; ++i) {
    float angle = (float)i * step * (3.14159f / 180.0f);
    float speed = 140.0f + (rand() % 240);
    Bullet *b = GetFreeBullet();
    if (b) {
      b->Fire(pos.x, pos.y, cosf(angle) * speed, sinf(angle) * speed, false,
              BulletType::BOSS);
      int starId = (rand() % 18) + 1;
      b->SetImage(ResourceManager::GetInstance().GetImage(
          L"Assets/Images/StarBullet/Star (" + std::to_wstring(starId) +
          L").png"));
      b->SetScale(0.5f);
      b->SetRadius(5.0f);
    }
  }
}

void Game::Reset() {
  m_score = 0;
  m_playerLife = 5;
  m_killCount = 0;
  m_isBossStage = false;
  m_bosses.clear();
  m_bgY = 0.0f;
  m_spawnTimer = 0.0f;
  m_isGameOver = false;
  m_stage = 1;
  m_prevStage = 1;
  m_bgTransitionAlpha = 1.0f;
  m_bgScrollSpeed = 150.0f;
  m_gameTimer = 0.0f;
  m_bossDifficulty = 1.0f;
  m_msgTimer = 0.0f;
  m_waveTimer = 0.0f;
  m_waveIndex = 0;
  m_waveSpawnCount = 0;
  m_spawnIntervalTimer = 0.0f;
  m_supernovaScale = 1.0f;
  m_totalRespawns = 0;
  m_state = GameState::START;

  m_enemies.clear();
  m_explosions.clear();
  m_items.clear();
  m_activeBullets.clear();
  m_enemiesToAdd.clear();
  m_itemsToAdd.clear();

  // Safety: Reset QPC time to now to prevent huge deltaTime on first frame
  // after restart
  QueryPerformanceCounter(&m_qpcLastTime);

  // Re-link bullet pool sustainably
  m_inactiveBullets.clear();
  for (auto &b : m_bulletPool) {
    b.SetActive(false);
    m_inactiveBullets.push_back(&b);
  }

  m_player.SetPosition((float)m_width / 2.0f, (float)m_height * 0.85f);
  
  // ?�테?��? ?�작 ?�림 추�?
  m_statusMsg = L"STAGE " + std::to_wstring(m_stage) + L" START";
  m_msgTimer = 3.5f;
  // Note: If m_player doesn't have a Reset(), we don't call it to avoid compile
  // errors. Assuming m_player's state is mostly managed by Game.
}
