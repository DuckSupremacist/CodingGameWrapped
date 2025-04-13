#include <array>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

// #define PRINT_DEBUG 0

using grid_t = uint64_t;
using depth_t = uint8_t;
using sum_t = uint8_t;
using tile_t = uint8_t;
using tile_index_t = size_t;

using raw_hash_t = std::array<uint32_t, 9>;
using hash_key_t = grid_t;

constexpr size_t GRID_SIDE_SIZE = 3;
constexpr size_t GRID_SIZE = GRID_SIDE_SIZE * GRID_SIDE_SIZE;

constexpr size_t TILE_WIDTH = 3;
constexpr grid_t TIGHT_TILE_MASK = (1 << TILE_WIDTH) - 1;

constexpr size_t TILE_GRID_OFFSET = TILE_WIDTH + 2; // 4 tiles could be summed up at once (4 = 1 << 2)
constexpr grid_t LOOSE_TILE_MASK = (1 << TILE_GRID_OFFSET) - 1;

constexpr size_t MAX_NEXT_POSITION_COUNT = 16;

constexpr size_t DEPTH_SHIFT = TILE_GRID_OFFSET * GRID_SIZE;

constexpr size_t HASH_WIDTH = 30;
constexpr uint32_t HASH_MASK = (1 << HASH_WIDTH) - 1;

std::string printFromGridT(grid_t grid)
{
    std::stringstream output;
    output << "Depth: " << (grid >> DEPTH_SHIFT) << std::endl;
    output << "Grid:" << std::endl << "\t";
    for (size_t i = 0; i < GRID_SIZE; i++)
    {
        output << std::dec << ((grid >> (i * TILE_GRID_OFFSET)) & LOOSE_TILE_MASK) << " ";
        if (i % GRID_SIDE_SIZE == 2)
        {
            output << std::endl << "\t";
        }
    }
    output << std::endl;
    return output.str();
}

std::string printFromRawHashT(raw_hash_t hash)
{
    std::stringstream output;
    output << "Hash:" << std::endl << "\t";
    for (size_t i = 0; i < GRID_SIZE; i++)
    {
        output << std::dec << hash[i] << " ";
        if (i % GRID_SIDE_SIZE == 2)
        {
            output << std::endl << "\t";
        }
    }
    output << std::endl;
    return output.str();
}

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

class Transformer
{
  public:
    enum TransformOperations : uint8_t
    {
        H = 0b001,
        V = 0b010,
        T = 0b100,
    };

    enum class Transformation : uint8_t
    {
        ID = 0,
        H_FLIP = H,
        V_FLIP = V,
        TRANSPOSE = T,
        ROT_180 = H | V,
        ROT_270 = H | T,
        ROT_90 = V | T,
        ANTI = H | V | T,
    };

  protected:
    static constexpr std::array<Transformation, 8> TRANSFORMS = {
        Transformation::ID, Transformation::H_FLIP, Transformation::V_FLIP, Transformation::ROT_90,
        Transformation::ROT_180, Transformation::ROT_270, Transformation::ANTI, Transformation::TRANSPOSE,
    };
};

class KeyTransformer : public Transformer
{
  public:
    static constexpr hash_key_t transform(hash_key_t key, Transformation t)
    {
        hash_key_t depth_part = key & (0xFFul << DEPTH_SHIFT);
        hash_key_t transformed_key = key;
        if (static_cast<TransformOperations>(t) & H)
        {
            transformed_key =
                ((transformed_key & 0b11111'11111'11111'00000'00000'00000'00000'00000'00000) >> (TILE_GRID_OFFSET * 6)
                ) |
                (transformed_key & 0b00000'00000'00000'11111'11111'11111'00000'00000'00000) |
                ((transformed_key & 0b00000'00000'00000'00000'00000'00000'11111'11111'11111) << (TILE_GRID_OFFSET * 6));
        }
        if (static_cast<TransformOperations>(t) & V)
        {
            transformed_key =
                ((transformed_key & 0b11111'00000'00000'11111'00000'00000'11111'00000'00000) >> (TILE_GRID_OFFSET * 2)
                ) |
                (transformed_key & 0b00000'11111'00000'00000'11111'00000'00000'11111'00000) |
                ((transformed_key & 0b00000'00000'11111'00000'00000'11111'00000'00000'11111) << (TILE_GRID_OFFSET * 2));
        }
        if (static_cast<TransformOperations>(t) & T)
        {
            transformed_key =
                (transformed_key & 0b11111'00000'00000'00000'11111'00000'00000'00000'11111) |
                ((transformed_key & 0b00000'11111'00000'00000'00000'11111'00000'00000'00000) >> (TILE_GRID_OFFSET * 2)
                ) |
                ((transformed_key & 0b00000'00000'00000'11111'00000'00000'00000'11111'00000)
                    << (TILE_GRID_OFFSET * 2)) |
                ((transformed_key & 0b00000'00000'11111'00000'00000'00000'00000'00000'00000) >> (TILE_GRID_OFFSET * 4)
                ) |
                ((transformed_key & 0b00000'00000'00000'00000'00000'00000'11111'00000'00000) << (TILE_GRID_OFFSET * 4));
        }
        return transformed_key | depth_part;
    }

    static constexpr hash_key_t reverseTransform(hash_key_t key, Transformation t)
    {
        if (t == Transformation::ROT_90)
        {
            return transform(key, Transformation::ROT_270);
        }
        if (t == Transformation::ROT_270)
        {
            return transform(key, Transformation::ROT_90);
        }
        return transform(key, t);
    }

    static constexpr std::pair<hash_key_t, Transformation> canonical(hash_key_t key)
    {
        // Find the smallest corner of the key
        const tile_t first_corner = (key >> TILE_GRID_OFFSET * 8) & TIGHT_TILE_MASK;
        const tile_t second_corner = (key >> TILE_GRID_OFFSET * 6) & TIGHT_TILE_MASK;
        const tile_t third_corner = (key >> TILE_GRID_OFFSET * 2) & TIGHT_TILE_MASK;
        const tile_t fourth_corner = key & TIGHT_TILE_MASK;
        const tile_t min_corner = std::min({first_corner, second_corner, third_corner, fourth_corner});
        // Find the transformation that gives the smallest corner as MSB
        size_t found = 0;
        Transformation transformation = Transformation::ID;
        if (min_corner == first_corner)
        {
            found++;
        }
        if (min_corner == second_corner)
        {
            found++;
            transformation = Transformation::V_FLIP;
        }
        if (min_corner == third_corner)
        {
            found++;
            transformation = Transformation::H_FLIP;
        }
        if (min_corner == fourth_corner)
        {
            found++;
            transformation = Transformation::ROT_180;
        }
        if (found == 1)
        {
            Transformation transposed =
                static_cast<Transformation>(static_cast<TransformOperations>(transformation) | T);
            // Check if the transposed key is smaller than the original key
            hash_key_t transposed_key = transform(key, transposed);
            hash_key_t transformation_key = transform(key, transformation);
            if (transposed_key < transformation_key)
            {
                return {transposed_key, transposed};
            }
            return {transformation_key, transformation};
        }

        // Two corners are the same, plan B
        // Loop through all transformations and find the one that gives the smallest corner
        hash_key_t canonical_key = key;
        transformation = Transformation::ID;
        for (Transformation t: TRANSFORMS)
        {
            hash_key_t transformed_key = transform(key, t);
            if (transformed_key < canonical_key)
            {
                canonical_key = transformed_key;
                transformation = t;
            }
        }
        return {canonical_key, transformation};
    }
};

class EntryTransformer : public Transformer
{
  public:
    static constexpr raw_hash_t transform(raw_hash_t raw_hash, Transformation t)
    {
        raw_hash_t transformed_hash = raw_hash;
        if (static_cast<TransformOperations>(t) & H)
        {
            transformed_hash = {
                transformed_hash[6], transformed_hash[7], transformed_hash[8], transformed_hash[3], transformed_hash[4],
                transformed_hash[5], transformed_hash[0], transformed_hash[1], transformed_hash[2],
            };
        }
        if (static_cast<TransformOperations>(t) & V)
        {
            transformed_hash = {
                transformed_hash[2], transformed_hash[1], transformed_hash[0], transformed_hash[5], transformed_hash[4],
                transformed_hash[3], transformed_hash[8], transformed_hash[7], transformed_hash[6],
            };
        }
        if (static_cast<TransformOperations>(t) & T)
        {
            transformed_hash = {
                transformed_hash[0], transformed_hash[3], transformed_hash[6], transformed_hash[1], transformed_hash[4],
                transformed_hash[7], transformed_hash[2], transformed_hash[5], transformed_hash[8],
            };
        }
        return transformed_hash;
    }

    static constexpr raw_hash_t reverseTransform(raw_hash_t raw_hash, Transformation t)
    {
        if (t == Transformation::ROT_90)
        {
            return transform(raw_hash, Transformation::ROT_270);
        }
        if (t == Transformation::ROT_270)
        {
            return transform(raw_hash, Transformation::ROT_90);
        }
        return transform(raw_hash, t);
    }
};

class RawHashLookup
{
  private:
    std::unordered_map<hash_key_t, raw_hash_t> _hash_lookup;

  public:
    std::optional<raw_hash_t> retrieve(hash_key_t key)
    {
        auto [transformed_key, t] = KeyTransformer::canonical(key);
        auto it = _hash_lookup.find(transformed_key);
        if (it != _hash_lookup.end())
        {
#ifdef BUILD_TESTS
            statistics.states_retrieved_from_hash_map++;
#ifdef PRINT_DEBUG
            std::cerr << "Retrieved hash for key:" << std::endl;
            std::cerr << printFromGridT(key);
            std::cerr << printFromRawHashT(it->second);
            std::cerr << "Transformed hash:" << std::endl;
            std::cerr << "Transformation: " << static_cast<int>(t) << std::endl;
            std::cerr << printFromGridT(transformed_key);
            std::cerr << printFromRawHashT(_hash_lookup[transformed_key]);
#endif
#endif
            return EntryTransformer::reverseTransform(it->second, t);
        }
        return std::nullopt;
    }

    void store(hash_key_t key, raw_hash_t hash)
    {
        auto [transformed_key, t] = KeyTransformer::canonical(key);
        _hash_lookup[transformed_key] = EntryTransformer::transform(hash, t);
#ifdef BUILD_TESTS
        statistics.states_stored_in_hash_map++;
#ifdef PRINT_DEBUG
        std::cerr << "Stored hash for key:" << std::endl;
        std::cerr << printFromGridT(key);
        std::cerr << printFromRawHashT(hash);
        std::cerr << "Transformed hash:" << std::endl;
        std::cerr << "Transformation: " << static_cast<int>(t) << std::endl;
        std::cerr << printFromGridT(transformed_key);
        std::cerr << printFromRawHashT(_hash_lookup[transformed_key]);
#endif
#endif
    }

    void clear()
    { _hash_lookup.clear(); }
};

class Position
{
  private:
    grid_t _grid;

    static RawHashLookup _hash_lookup;

    struct NextPositions
    {
        std::array<grid_t, MAX_NEXT_POSITION_COUNT> positions;
        size_t count;

        void emplace_back(grid_t position) // NOLINT
        {
            if (count < MAX_NEXT_POSITION_COUNT)
            {
                positions[count++] = position;
            }
        }

        [[nodiscard]] bool empty() const
        { return count == 0; }
    };

    [[nodiscard]] constexpr NextPositions nextPositions() const
    {
        depth_t depth = (_grid >> DEPTH_SHIFT);
        // If the distance from depth is 0, return an empty vector
        if (depth == 0)
        {
            return {};
        }

        const grid_t &offset_a = _grid;
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

            constexpr SumCombo(sum_t sum, grid_t mask) : sum_code(sum), success_mask(mask)
            {}
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
            std::pair{SUM_AB, 1}, std::pair{SUM_ABC, 0}, std::pair{SUM_AC, 1},
            std::pair{SUM_ACD, 0}, std::pair{SUM_ABCD, 1}, std::pair{SUM_ABD, 2},
            std::pair{SUM_AC, 3}, std::pair{SUM_ABC, 4}, std::pair{SUM_AB, 5},
        };

        NextPositions next_positions{};
        for (tile_index_t i = 0; i < GRID_SIZE; ++i)
        {
            const tile_t tile = (_grid >> (i * TILE_GRID_OFFSET)) & TIGHT_TILE_MASK;
            if (tile != 0)
            {
                continue;
            }
            bool found_sum = false;
            for (const auto &[sum_combo, sum]: sums)
            {
                if ((sum_combo.sum_code & TILE_SUM_COMBINATIONS[i].first) ==
                    sum_combo.sum_code) // Verify sum is valid for this tile
                {
                    const grid_t target_mask = sum_combo.success_mask
                        << (TILE_SUM_COMBINATIONS[i].second * TILE_GRID_OFFSET);
                    const grid_t target_presence_mask = target_mask & CHECK_PRESENCE_MASK;
                    if ((value_present & target_presence_mask) != target_presence_mask)
                    {
                        // No value to sum
                        continue;
                    }
                    tile_t sum_value = sum >> (TILE_SUM_COMBINATIONS[i].second * TILE_GRID_OFFSET) & LOOSE_TILE_MASK;
                    if (sum_value <= 6)
                    {
                        found_sum = true;
                        grid_t new_grid = _grid & ~target_mask;
                        new_grid |= (static_cast<grid_t>(sum_value) << (i * TILE_GRID_OFFSET));
                        new_grid -= (1ul << DEPTH_SHIFT);
                        next_positions.emplace_back(new_grid);
                    }
                }
            }
            if (!found_sum)
            {
                grid_t new_grid = _grid | (1ul << (i * TILE_GRID_OFFSET));
                new_grid -= (1ul << DEPTH_SHIFT);
                next_positions.emplace_back(new_grid);
            }
        }
        return next_positions;
    }

  public:
    static void clearHashLookup()
    { _hash_lookup.clear(); }

    explicit Position(grid_t position) : _grid(position)
    {
#ifdef BUILD_TESTS
#ifdef PRINT_DEBUG
        std::cerr << "Position created:" << std::endl;
        std::cerr << printFromGridT(_grid);
#endif
#endif
    }

    [[nodiscard]] raw_hash_t produceRawHash() const
    {
        raw_hash_t hash{};
        for (size_t i = 0; i < GRID_SIZE; i++)
        {
            hash[i] = (_grid >> (i * TILE_GRID_OFFSET)) & TIGHT_TILE_MASK;
        }
        return hash;
    }

    [[nodiscard]] uint32_t outputHash()
    {
        raw_hash_t hash = calculateHash();
        uint32_t clean_hash = 0;
        for (size_t i = 0; i < GRID_SIZE; i++)
        {
            clean_hash *= 10;
            clean_hash += hash[i];
        }
        return clean_hash & HASH_MASK;
    }

    raw_hash_t calculateHash()
    {
        {
            std::optional<raw_hash_t> hash = _hash_lookup.retrieve(_grid);
            if (hash.has_value())
            {
                return hash.value();
            }
        }

        raw_hash_t hash{};
        NextPositions next_positions = nextPositions();
        if (next_positions.empty())
        {
            hash = produceRawHash();
#ifdef BUILD_TESTS
            statistics.end_states_count++;
#endif
            _hash_lookup.store(_grid, hash);
            return hash;
        }
        for (size_t i = 0; i < next_positions.count; i++)
        {
            grid_t next_grid = next_positions.positions[i];
            Position next_position(next_grid);
            // Sum the hash of the next position
            raw_hash_t next_hash = next_position.calculateHash();
            for (size_t j = 0; j < GRID_SIZE; j++)
            {
                hash[j] += next_hash[j];
            }
        }
        _hash_lookup.store(_grid, hash);
        return hash;
    }
};

RawHashLookup Position::_hash_lookup;

grid_t getInitialPosition()
{
    grid_t grid = 0;
    int value;
    std::cin >> value;
    std::cin.ignore();

    depth_t distance_from_depth = value;

    for (size_t i = 0; i < GRID_SIZE; i++)
    {
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
    uint32_t hash = initial_position.outputHash();
    std::cout << hash << std::endl;
#ifdef BUILD_TESTS
    statistics.printStatistics();
    Position::clearHashLookup();
#endif
    return 0;
}
