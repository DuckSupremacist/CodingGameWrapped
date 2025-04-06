#include <array>
#include <iostream>
#include <span>
#include <string>
#include <vector>

using tile_index_t = long;
constexpr int GRID_SIDE_SIZE = 3;
constexpr int GRID_SIZE = GRID_SIDE_SIZE * GRID_SIDE_SIZE;
constexpr uint32_t HASH_MODULO = 1 << 30;

using tile_t = uint8_t;
typedef std::array<tile_t, GRID_SIZE> Grid;

class Position
{
  private:
    size_t _distance_from_depth;
    Grid _grid;
    uint32_t _hash;

    bool sumCandidates(const std::vector<tile_index_t>::iterator first, const std::vector<tile_index_t>::iterator end, const tile_index_t target_tile_index, const Grid& grid, std::vector<Position>& next_positions, tile_t sum = 0)
    {
        bool found = false;
        for (auto it = first; it != end; ++it)
        {
            tile_index_t tile_index = *it;
            tile_t candidate_sum = sum + _grid[tile_index];

            if (candidate_sum > 6)
            {
                continue;
            }

            Grid next_grid = grid;
            next_grid[target_tile_index] = candidate_sum;
            next_grid[tile_index] = 0;
            if (sum != 0)
            {
                found = true;
                next_positions.emplace_back(_distance_from_depth - 1, next_grid);
            }

            if (sumCandidates(it + 1, end, target_tile_index, next_grid, next_positions, candidate_sum))
            {
                found = true;
            }
        }
        return found;
    }

    std::vector<Position> nextPositions()
    {
        // If the distance from depth is 0, return an empty vector
        if (_distance_from_depth == 0)
        {
            return {};
        }
        std::vector<Position> next_positions;
        for (tile_index_t i = 0; i < GRID_SIZE; ++i)
        {
            tile_t& tile = _grid[i];
            if (tile != 0)
            {
                continue;
            }

            std::vector<tile_index_t> candidate_tiles;

            // Left
            if (i % GRID_SIDE_SIZE > 0)
            {
                tile_t& candidate_tile = _grid[i - 1];
                if (candidate_tile != 0 && candidate_tile != 6)
                {
                    candidate_tiles.push_back(i - 1);
                }
            }

            // Right
            if (i % GRID_SIDE_SIZE < 2)
            {
                tile_t& candidate_tile = _grid[i + 1];
                if (candidate_tile != 0 && candidate_tile != 6)
                {
                    candidate_tiles.push_back(i + 1);
                }
            }

            // Up
            if (i >= GRID_SIDE_SIZE)
            {
                tile_t& candidate_tile = _grid[i - GRID_SIDE_SIZE];
                if (candidate_tile != 0 && candidate_tile != 6)
                {
                    candidate_tiles.push_back(i - GRID_SIDE_SIZE);
                }
            }

            // Down
            if (i < GRID_SIZE - GRID_SIDE_SIZE)
            {
                tile_t& candidate_tile = _grid[i + GRID_SIDE_SIZE];
                if (candidate_tile != 0 && candidate_tile != 6)
                {
                    candidate_tiles.push_back(i + GRID_SIDE_SIZE);
                }
            }

            // Not enough candidates to sum
            if (candidate_tiles.size() < 2 || !sumCandidates(candidate_tiles.begin(), candidate_tiles.end(), i, _grid, next_positions))
            {
                // If no candidates were found, create a new position with the current tile set to 1
                Grid next_grid = _grid;
                next_grid[i] = 1;
                next_positions.emplace_back(_distance_from_depth - 1, next_grid);
                continue;
            }
        }
        return next_positions;
    }

    size_t calculateHash()
    {
        std::vector<Position> next_positions = nextPositions();
        if (next_positions.empty())
        {
            _hash = 0;
            for (const tile_t& tile : _grid)
            {
                _hash = _hash * 10 + tile;
            }
            _hash %= HASH_MODULO;
            return 1;
        }
        _hash = 0;
        for (const Position& position : next_positions)
        {
            _hash += position.getHash();
            _hash %= HASH_MODULO;
        }
        return next_positions.size();
    }

  public:
    Position(size_t distance_from_depth, Grid position) :
        _distance_from_depth(distance_from_depth),
        _grid(position),
        _hash(0)
    {
        calculateHash();
    }

    explicit Position() : _distance_from_depth(0), _grid{0}, _hash(0)
    {
        std::cin >> _distance_from_depth;
        std::cin.ignore();
        std::cerr << "Depth: " << _distance_from_depth << std::endl;
        for (tile_t& tile : _grid)
        {
            int value;

            std::cin >> value;
            tile = static_cast<tile_t>(value);
            std::cin.ignore();
        }
        std::cerr << "Initial position: ";
        for (const tile_t& tile : _grid)
        {
            std::cerr << static_cast<int>(tile);
        }
        std::cerr << std::endl;
        std::cerr << calculateHash() << " end positions." << std::endl;
    }

    [[nodiscard]] uint32_t getHash() const
    {
        return _hash;
    }
};

#ifdef BUILD_TESTS
int testMain()
#else

int main()
#endif
{
    Position initial_position{};
    std::cout << initial_position.getHash() << std::endl;

    return 0;
}
