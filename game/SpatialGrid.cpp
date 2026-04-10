#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "SpatialGrid.h"
#include <algorithm>

SpatialGrid::SpatialGrid(int screenWidth, int screenHeight, int cellSize)
    : m_screenWidth(screenWidth), m_screenHeight(screenHeight), m_cellSize(cellSize) {
    m_cols = (m_screenWidth + m_cellSize - 1) / m_cellSize;
    m_rows = (m_screenHeight + m_cellSize - 1) / m_cellSize;
    m_cells.resize(m_cols, std::vector<std::vector<Entity*>>(m_rows));
}

void SpatialGrid::Clear() {
    for (int i = 0; i < m_cols; ++i) {
        for (int j = 0; j < m_rows; ++j) {
            m_cells[i][j].clear();
        }
    }
}

void SpatialGrid::Insert(Entity* entity) {
    if (!entity || !entity->IsActive()) return;

    Vector2 pos = entity->GetPosition();
    int col = static_cast<int>(pos.x) / m_cellSize;
    int row = static_cast<int>(pos.y) / m_cellSize;

    if (col >= 0 && col < m_cols && row >= 0 && row < m_rows) {
        m_cells[col][row].push_back(entity);
    }
}

void SpatialGrid::GetNearbyEntities(float x, float y, float radius, std::vector<Entity*>& outEntities) {
    int startCol = std::max(0, static_cast<int>((x - radius) / (float)m_cellSize));
    int endCol = std::min(m_cols - 1, static_cast<int>((x + radius) / (float)m_cellSize));
    int startRow = std::max(0, static_cast<int>((y - radius) / (float)m_cellSize));
    int endRow = std::min(m_rows - 1, static_cast<int>((y + radius) / (float)m_cellSize));

    for (int i = startCol; i <= endCol; ++i) {
        for (int j = startRow; j <= endRow; ++j) {
            for (auto* entity : m_cells[i][j]) {
                outEntities.push_back(entity);
            }
        }
    }
}
