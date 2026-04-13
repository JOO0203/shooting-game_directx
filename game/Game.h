#pragma once
#include <vector>
#include <string>
#include <memory>
#include "D3D11Renderer.h"
#include "Entity.h"
#include "Player.h"
#include "Bullet.h"
#include "Enemy.h"
#include "Explosion.h"
#include "Item.h"
#include "Boss.h"
#include "SpatialGrid.h"

enum class GameState { START, PLAYING, PAUSED, GAMEOVER, GAMECLEAR, ENDING, RANKING };

class Game {
public:
    Game();
    ~Game();
    bool Initialize(HWND hWnd);
    void Reset();
    void Run();
    void Cleanup();

private:
    std::unique_ptr<D3D11Renderer> m_renderer;
    HWND m_hWnd = NULL;
    int m_width = 1024, m_height = 1024;

    GameState m_state;
    GameState m_prevState; // State before pausing

    void Update(float deltaTime);
    void Render();
    float m_waveTimer = 0.0f;
    int m_waveIndex = 0;
    int m_waveSpawnCount = 0;   
    float m_spawnIntervalTimer = 0.0f;

    void FirePlayerBullet();
    void FireAutoSubWeapon(bool isPlasma);
    void FireEnemyBullet(Enemy* enemy);
    void FireBossBullet(Boss* boss, float deltaTime);
    void FireSubBossDeathPattern(Vector2 pos);
    void FireEnemyRadial(Vector2 pos, int count, float speed, BulletType type = BulletType::ENEMY);
    void CheckCollisions();
    Bullet* GetFreeBullet();
    std::wstring GetBackgroundPath(int stage);
    void SpawnPattern(float deltaTime);
    void LoadRankings();
    void SaveRankings();
    void UpdateRankings(int score);


    Player m_player;
    std::vector<std::unique_ptr<Boss>> m_bosses;
    std::vector<std::unique_ptr<Enemy>> m_enemies;
    std::vector<Bullet> m_bulletPool;
    std::vector<Bullet*> m_activeBullets;
    std::vector<Bullet*> m_inactiveBullets;
    std::vector<std::unique_ptr<Explosion>> m_explosions;
    std::vector<std::unique_ptr<Item>> m_items;
    std::vector<std::unique_ptr<Enemy>> m_enemiesToAdd;
    std::vector<std::unique_ptr<Item>> m_itemsToAdd;
    std::unique_ptr<SpatialGrid> m_grid;

    const int MAX_BULLETS = 6000;
    float m_fireTimer = 0.0f;
    float m_spawnTimer = 0.0f;
    int m_score = 0;
    int m_playerLife = 5;
    int m_killCount = 0;
    bool m_isBossStage = false;
    bool m_isGameOver = false;
    int m_stage = 1;
    int m_prevStage = 1;
    float m_bgTransitionAlpha = 1.0f;
    float m_endingTimer = 0.0f;
    float m_bgScrollSpeed = 150.0f;
    float m_bossRespawnTimer = 0.0f;
    float m_gameTimer = 0.0f;
    float m_bossDifficulty = 1.0f;
    float m_bossHitEffectTimer = 0.0f;
    std::wstring m_statusMsg = L"";
    float m_msgTimer = 0.0f;
    int m_totalRespawns = 0;
    float m_bgY = 0.0f;
    float m_supernovaScale = 1.0f;
    LARGE_INTEGER m_qpcFrequency = { 0 };
    LARGE_INTEGER m_qpcLastTime = { 0 };

    // FPS 측정 및 타임스텝 제어
    const float FIXED_DELTA_TIME = 1.0f / 70.0f;
    float m_accumulator   = 0.0f;
    int   m_currentFps    = 0;
    int   m_fpsFrameCount = 0;
    float m_fpsTimer      = 0.0f;

    // 랭킹 및 메뉴
    std::vector<int> m_rankings;
    int m_menuIndex = 0;
};