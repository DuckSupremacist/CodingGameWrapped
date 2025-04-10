#include <array>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>

using grid_t = uint64_t;
using depth_t = uint8_t;
using sum_t = uint8_t;
using tile_t = uint8_t;
using tile_index_t = size_t;

using hash_t = uint32_t;
using hash_key_t = grid_t;

constexpr size_t GRID_SIDE_SIZE = 3;
constexpr size_t GRID_SIZE = GRID_SIDE_SIZE * GRID_SIDE_SIZE;

constexpr size_t TILE_WIDTH = 3;
constexpr grid_t TIGHT_TILE_MASK = (1 << TILE_WIDTH) - 1;

constexpr size_t TILE_GRID_OFFSET = TILE_WIDTH + 2; // 4 tiles could be summed up at once (4 = 1 << 2)
constexpr grid_t LOOSE_TILE_MASK = (1 << TILE_GRID_OFFSET) - 1;

constexpr grid_t GRID_MASK = (1ul << (TILE_GRID_OFFSET * GRID_SIZE)) - 1;

constexpr size_t MAX_NEXT_POSITION_COUNT = 16;

constexpr size_t DEPTH_SHIFT = TILE_GRID_OFFSET * GRID_SIZE;

constexpr size_t HASH_WIDTH = 30;
constexpr uint32_t HASH_MASK = (1 << HASH_WIDTH) - 1;

typedef std::array<tile_t, GRID_SIZE> Grid;

#ifdef BUILD_TESTS
struct Statistics
{
    size_t end_states_count;
    size_t states_stored_in_hash_map;
    size_t states_retrieved_from_hash_map;

    void printStatistics() const {
        std::cerr << "End states count: " << end_states_count << std::endl;
        std::cerr << "States stored in hash map: " << states_stored_in_hash_map << std::endl;
        std::cerr << "States retrieved from hash map: " << states_retrieved_from_hash_map << std::endl;
    }
} statistics;
#endif

class Position
{
  private:
    const grid_t _grid;

    static std::unordered_map<hash_key_t, hash_t> _hash_lookup;

    struct NextPositions
    {
        std::array<grid_t, MAX_NEXT_POSITION_COUNT> positions;
        size_t count;

        void emplace_back(grid_t position) // NOLINT
        {
            if (count < MAX_NEXT_POSITION_COUNT) {
                positions[count++] = position;
            }
        }

        [[nodiscard]] bool empty() const { return count == 0; }
    };

    [[nodiscard]] constexpr NextPositions nextPositions() const {
        depth_t depth = (_grid >> DEPTH_SHIFT);
        // If the distance from depth is 0, return an empty vector
        if (depth == 0) {
            return {};
        }

        const grid_t& offset_a = _grid;
        const grid_t offset_b = offset_a >> (TILE_GRID_OFFSET * 2);
        const grid_t offset_c = offset_a >> (TILE_GRID_OFFSET * 4);
        const grid_t offset_d = offset_a >> (TILE_GRID_OFFSET * 6);

        const grid_t value_present = offset_a | offset_a >> 1 | offset_a >> 2;

        enum : sum_t
        {
            SUM_AB = 0b1100,
            SUM_AC = 0b1010,
            SUM_AD = 0b1001,
            SUM_BC = 0b0110,
            SUM_BD = 0b0101,
            SUM_CD = 0b0011,
            SUM_ABC = 0b1110,
            SUM_ABD = 0b1101,
            SUM_ACD = 0b1011,
            SUM_BCD = 0b0111,
            SUM_ABCD = 0b1111,
        };
        struct SumCombo
        {
            sum_t sum_code;
            grid_t success_mask;

            constexpr SumCombo(sum_t sum, grid_t mask) : sum_code(sum), success_mask(mask) {}
        };
        constexpr grid_t MASK_OFFSET_A = LOOSE_TILE_MASK;
        constexpr grid_t MASK_OFFSET_B = LOOSE_TILE_MASK << (TILE_GRID_OFFSET * 2);
        constexpr grid_t MASK_OFFSET_C = LOOSE_TILE_MASK << (TILE_GRID_OFFSET * 4);
        constexpr grid_t MASK_OFFSET_D = LOOSE_TILE_MASK << (TILE_GRID_OFFSET * 6);

        constexpr grid_t CHECK_PRESENCE_MASK = 1ul | (1ul << TILE_GRID_OFFSET) | (1ul << (TILE_GRID_OFFSET * 2)) |
                                               (1ul << (TILE_GRID_OFFSET * 3)) | (1ul << (TILE_GRID_OFFSET * 4)) |
                                               (1ul << (TILE_GRID_OFFSET * 5)) | (1ul << (TILE_GRID_OFFSET * 6)) |
                                               (1ul << (TILE_GRID_OFFSET * 7)) | (1ul << (TILE_GRID_OFFSET * 8));

        constexpr grid_t MASK_SUM_AB = MASK_OFFSET_A | MASK_OFFSET_B;
        constexpr grid_t MASK_SUM_AC = MASK_OFFSET_A | MASK_OFFSET_C;
        constexpr grid_t MASK_SUM_AD = MASK_OFFSET_A | MASK_OFFSET_D;
        constexpr grid_t MASK_SUM_BC = MASK_OFFSET_B | MASK_OFFSET_C;
        constexpr grid_t MASK_SUM_BD = MASK_OFFSET_B | MASK_OFFSET_D;
        constexpr grid_t MASK_SUM_CD = MASK_OFFSET_C | MASK_OFFSET_D;
        constexpr grid_t MASK_SUM_ABC = MASK_OFFSET_A | MASK_OFFSET_B | MASK_OFFSET_C;
        constexpr grid_t MASK_SUM_ABD = MASK_OFFSET_A | MASK_OFFSET_B | MASK_OFFSET_D;
        constexpr grid_t MASK_SUM_ACD = MASK_OFFSET_A | MASK_OFFSET_C | MASK_OFFSET_D;
        constexpr grid_t MASK_SUM_BCD = MASK_OFFSET_B | MASK_OFFSET_C | MASK_OFFSET_D;
        constexpr grid_t MASK_SUM_ABCD = MASK_OFFSET_A | MASK_OFFSET_B | MASK_OFFSET_C | MASK_OFFSET_D;

        const std::array<std::pair<SumCombo, grid_t>, 11> sums = {
            std::pair{SumCombo{SUM_AB, MASK_SUM_AB}, offset_a + offset_b},
            std::pair{SumCombo{SUM_AC, MASK_SUM_AC}, offset_a + offset_c},
            std::pair{SumCombo{SUM_AD, MASK_SUM_AD}, offset_a + offset_d},
            std::pair{SumCombo{SUM_BC, MASK_SUM_BC}, offset_b + offset_c},
            std::pair{SumCombo{SUM_BD, MASK_SUM_BD}, offset_b + offset_d},
            std::pair{SumCombo{SUM_CD, MASK_SUM_CD}, offset_c + offset_d},
            std::pair{SumCombo{SUM_ABC, MASK_SUM_ABC}, offset_a + offset_b + offset_c},
            std::pair{SumCombo{SUM_ABD, MASK_SUM_ABD}, offset_a + offset_b + offset_d},
            std::pair{SumCombo{SUM_ACD, MASK_SUM_ACD}, offset_a + offset_c + offset_d},
            std::pair{SumCombo{SUM_BCD, MASK_SUM_BCD}, offset_b + offset_c + offset_d},
            std::pair{SumCombo{SUM_ABCD, MASK_SUM_ABCD}, offset_a + offset_b + offset_c + offset_d},
        };

        constexpr std::array<std::pair<sum_t, size_t>, GRID_SIZE> TILE_SUM_COMBINATIONS = {
            std::pair{SUM_AB, 1},  std::pair{SUM_ABC, 0},  std::pair{SUM_AC, 1},
            std::pair{SUM_ACD, 0}, std::pair{SUM_ABCD, 1}, std::pair{SUM_ABD, 2},
            std::pair{SUM_AC, 3},  std::pair{SUM_ABC, 4},  std::pair{SUM_AB, 5},
        };

        NextPositions next_positions{};
        for (tile_index_t i = 0; i < GRID_SIZE; ++i) {
            const tile_t tile = (_grid >> (i * TILE_GRID_OFFSET)) & TIGHT_TILE_MASK;
            if (tile != 0) {
                continue;
            }
            bool found_sum = false;
            for (const auto& [sum_combo, sum] : sums) {
                if ((sum_combo.sum_code & TILE_SUM_COMBINATIONS[i].first) ==
                    sum_combo.sum_code) // Verify sum is valid for this tile
                {
                    const grid_t target_mask = sum_combo.success_mask
                                               << (TILE_SUM_COMBINATIONS[i].second * TILE_GRID_OFFSET);
                    const grid_t target_presence_mask = target_mask & CHECK_PRESENCE_MASK;
                    if ((value_present & target_presence_mask) != target_presence_mask) {
                        // No value to sum
                        continue;
                    }
                    tile_t sum_value = sum >> (TILE_SUM_COMBINATIONS[i].second * TILE_GRID_OFFSET) & LOOSE_TILE_MASK;
                    if (sum_value <= 6) {
                        found_sum = true;
                        grid_t new_grid = _grid & ~target_mask;
                        new_grid |= (static_cast<grid_t>(sum_value) << (i * TILE_GRID_OFFSET));
                        new_grid -= (1ul << DEPTH_SHIFT);
                        next_positions.emplace_back(new_grid);
                    }
                }
            }
            if (!found_sum) {
                grid_t new_grid = _grid | (1ul << (i * TILE_GRID_OFFSET));
                new_grid -= (1ul << DEPTH_SHIFT);
                next_positions.emplace_back(new_grid);
            }
        }
        return next_positions;
    }

  public:
    void printGrid(grid_t grid) const {
        std::cerr << "Depth: " << (grid >> DEPTH_SHIFT) << std::endl;
        std::cerr << "Grid:" << std::endl << "\t";
        for (size_t i = GRID_SIZE; i > 0; i--) {
            std::cerr << std::dec << ((grid >> ((i - 1) * TILE_GRID_OFFSET)) & LOOSE_TILE_MASK) << " ";
            if (i % GRID_SIDE_SIZE == 1) {
                std::cerr << std::endl << "\t";
            }
        }
        std::cerr << std::endl;
    }

    void printGrid() const { printGrid(_grid); }

    static void clearHashLookup() { _hash_lookup.clear(); }

    explicit Position(grid_t position) : _grid(position) {}

    hash_t calculateHash() {
        if (_hash_lookup.find(_grid) != _hash_lookup.end()) {
#ifdef BUILD_TESTS
            statistics.states_retrieved_from_hash_map++;
#endif
            return _hash_lookup[_grid];
        }

        hash_t hash = 0;
        NextPositions next_positions = nextPositions();
        if (next_positions.empty()) {
            for (size_t i = 0; i < GRID_SIZE; i++) {
                tile_t tile = (_grid >> (i * TILE_GRID_OFFSET)) & TIGHT_TILE_MASK;
                hash = hash * 10 + tile;
            }
#ifdef BUILD_TESTS
            statistics.states_stored_in_hash_map++;
            statistics.end_states_count++;
#endif
            _hash_lookup[_grid] = hash;
            return hash;
        }
        for (size_t i = 0; i < next_positions.count; i++) {
            grid_t next_grid = next_positions.positions[i];
            Position next_position(next_grid);
            hash += next_position.calculateHash();
        }
        hash &= HASH_MASK;
#ifdef BUILD_TESTS
        statistics.states_stored_in_hash_map++;
#endif
        _hash_lookup[_grid] = hash;
        return hash;
    }
};

std::unordered_map<hash_key_t, hash_t> Position::_hash_lookup;

grid_t getInitialPosition() {
    grid_t grid = 0;
    int value;
    std::cin >> value;
    std::cin.ignore();

    depth_t distance_from_depth = value;

    for (size_t i = 0; i < GRID_SIZE; i++) {
        std::cin >> value;
        std::cin.ignore();

        grid |= (value & TIGHT_TILE_MASK) << (i * TILE_GRID_OFFSET);
    }
    grid |= (static_cast<uint64_t>(distance_from_depth) << DEPTH_SHIFT);

    return grid;
}

#ifdef BUILD_TESTS
int testMain()
#else

int main()
#endif
{
    Position initial_position{getInitialPosition()};
    uint32_t hash = initial_position.calculateHash();
    std::cout << hash << std::endl;
#ifdef BUILD_TESTS
    statistics.printStatistics();
    Position::clearHashLookup();
#endif
    return 0;
}
