#include <array>
#include <iostream>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

using tile_index_t = long;
using hash_t = uint32_t;
using hash_key_t = uint64_t;
using tile_t = uint8_t;

constexpr int GRID_SIDE_SIZE = 3;
constexpr int GRID_SIZE = GRID_SIDE_SIZE * GRID_SIDE_SIZE;
constexpr size_t HASH_WIDTH = 30;
constexpr uint32_t HASH_MODULO = 1 << HASH_WIDTH;
constexpr size_t DEPTH_SHIFT = HASH_WIDTH;
constexpr uint64_t GRID_MASK = (1 << (GRID_SIZE * 3)) - 1;

static_assert(GRID_MASK == 0b111'111'111'111'111'111'111'111'111, "GRID_MASK should be 9 * 3 bits");

constexpr size_t TILE_SHIFT = 3;
typedef std::array<tile_t, GRID_SIZE> Grid;

//class HashLookup
//{
//    static std::unordered_map<hash_key_t, hash_t> _hash;
//
//    // 3×3 tile index permutations (forward transforms):
//    static constexpr int ID[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
//    static constexpr int R90[9] = {6, 3, 0, 7, 4, 1, 8, 5, 2};
//    static constexpr int R180[9] = {8, 7, 6, 5, 4, 3, 2, 1, 0};
//    static constexpr int R270[9] = {2, 5, 8, 1, 4, 7, 0, 3, 6};
//    static constexpr int RVERT[9] = {2, 1, 0, 5, 4, 3, 8, 7, 6};
//    static constexpr int RHORZ[9] = {6, 7, 8, 3, 4, 5, 0, 1, 2};
//    static constexpr int RMAIN[9] = {0, 3, 6, 1, 4, 7, 2, 5, 8};
//    static constexpr int RANTI[9] = {8, 5, 2, 7, 4, 1, 6, 3, 0};
//
//    static constexpr std::array<std::span<const int>, 8> TRANSFORMS = {
//        ID, R90, R180, R270, RVERT, RHORZ, RMAIN, RANTI};
//
//    // Inverse transforms (swapping R90 and R270):
//    static constexpr std::array<std::span<const int>, 8> INVERSE_TRANSFORMS = {
//        ID, R270, R180, R90, RVERT, RHORZ, RANTI, RMAIN};
//
//    static constexpr inline uint8_t depth(hash_key_t k)
//    { return k >> DEPTH_SHIFT; }
//
//    static constexpr inline void setDepth(hash_key_t &k, uint8_t d)
//    {
//        k &= ~(0xFFu << DEPTH_SHIFT);
//        k |= (hash_key_t(d) & 0xFFu) << DEPTH_SHIFT;
//    }
//
//    static constexpr inline uint8_t tile(hash_key_t k, int i)
//    {
//        return (k >> ((8 - i) * 3)) & 0xb111u;
//    }
//
//    static constexpr inline void setTile(hash_key_t &k, int i, uint8_t v)
//    {
//        int sh = (8 - i) * 3;
//        k &= ~(0xb111u << sh);
//        k |= (hash_key_t(v & 0xb111u) << sh);
//    }
//
//    static constexpr hash_key_t transform(hash_key_t in, int t)
//    {
//        hash_key_t out = 0;
//        setDepth(out, depth(in));
//        for (int i = 0; i < 9; ++i)
//        { setTile(out, i, tile(in, TRANSFORMS[t][i])); }
//        return out;
//    }
//
//    static constexpr hash_key_t inverseTransform(hash_key_t in, int t)
//    {
//        hash_key_t out = 0;
//        setDepth(out, depth(in));
//        for (int i = 0; i < 9; ++i) setTile(out, i, tile(in, INVERSE_TRANSFORMS[t][i]));
//        return out;
//    }
//
//    static constexpr std::pair<hash_key_t, int> canonic(hash_key_t k)
//    {
//        auto best = transform(k, 0);
//        int idx = 0;
//        for (int t = 1; t < 8; ++t)
//        {
//            auto cand = transform(k, t);
//            if (cand < best)
//            {
//                best = cand;
//                idx = t;
//            }
//        }
//        return {best, idx};
//    }
//
//  public:
//    [[nodiscard]] static uint32_t findHash(hash_key_t key)
//    {
//        auto [canKey, t] = canonic(key);
//        auto it = _hash.find(canKey);
//        if(it == _hash.end()) return 0;
//        return inverseTransform(it->second, t); // inverse transform
//    }
//
//    static void storeHash(hash_key_t key, uint32_t val)
//    {
//        auto [canKey, t] = canonic(key);
//        _hash[canKey] = transform(val, t); // forward transform
//    }
//
//    static void clearHashLookup()
//    {
//        _hash.clear();
//    }
//};
//
//std::unordered_map<hash_key_t, hash_t> HashLookup::_hash;

class Position
{
  private:
    size_t _distance_from_depth;
    Grid _grid;
    hash_t _hash;
    hash_key_t _key;

    void computeKey()
    {
        _key = 0;
        for (const tile_t& tile : _grid)
        {
            _key = (_key << TILE_SHIFT) | tile;
        }
        _key |= (_distance_from_depth << DEPTH_SHIFT);
    }

    static std::unordered_map<hash_key_t, hash_t> _hash_lookup;

    bool constexpr sumCandidates(const std::vector<tile_index_t>::iterator first, const std::vector<tile_index_t>::iterator end, const tile_index_t target_tile_index, const Grid& grid, std::vector<Position>& next_positions, tile_t sum = 0)
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

    void calculateHash()
    {
        if (_hash_lookup.find(_key) != _hash_lookup.end())
        {
            _hash = _hash_lookup[_key];
            return;
        }

        std::vector<Position> next_positions = nextPositions();
        if (next_positions.empty())
        {
            _hash = 0;
            for (const tile_t& tile : _grid)
            {
                _hash = _hash * 10 + tile;
            }
            _hash_lookup[_key] = _hash;
            return;
        }
        _hash = 0;
        for (const Position& position : next_positions)
        {
            _hash += position.getHash();
            _hash %= HASH_MODULO;
        }

        _hash_lookup[_key] = _hash;
    }

  public:
    static void clearHashLookup()
    {
        _hash_lookup.clear();
    }
    Position(size_t distance_from_depth, Grid position) :
        _distance_from_depth(distance_from_depth),
        _grid(position)
    {
        computeKey();
        calculateHash();
    }

    explicit Position() : _distance_from_depth(0), _grid{0}
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
        computeKey();
        calculateHash();
    }

    [[nodiscard]] uint32_t getHash() const
    {
        return _hash;
    }
};

std::unordered_map<hash_key_t, hash_t> Position::_hash_lookup;

#ifdef BUILD_TESTS
int testMain()
#else

int main()
#endif
{
    Position initial_position{};
    std::cout << initial_position.getHash() << std::endl;

#ifdef BUILD_TESTS
    Position::clearHashLookup();
//    HashLookup::clearHashLookup();
#endif
    return 0;
}
