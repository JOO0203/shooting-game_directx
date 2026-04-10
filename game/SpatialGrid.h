#pragma once
#include <vector>
#include "Entity.h"

class SpatialGrid {
public:
    SpatialGrid(int screenWidth, int screenHeight, int cellSize);
    ~SpatialGrid() = default;

    void Clear();
    void Insert(Entity* entity);
    void GetNearbyEntities(float x, float y, float radius, std::vector<Entity*>& outEntities);

private:
    int m_screenWidth;
    int m_screenHeight;
    int m_cellSize;
    int m_cols;
    int m_rows;

    std::vector<std::vector<std::vector<Entity*>>> m_cells;
};
