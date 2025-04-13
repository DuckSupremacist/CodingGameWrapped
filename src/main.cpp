#include <array>
#include <functional>
#include <iostream>
#include <optional>
#ifdef BUILD_TESTS
#include <sstream>
#endif
#include <string>
#include <unordered_map>
#include <vector>

// --------------------------------------------------------------------------
// Type definitions and constants
// --------------------------------------------------------------------------

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

constexpr size_t TILE_GRID_OFFSET = TILE_WIDTH + 2; // 5 bits per tile slot
constexpr grid_t LOOSE_TILE_MASK = (1 << TILE_GRID_OFFSET) - 1;

constexpr size_t MAX_NEXT_POSITION_COUNT = 16;

constexpr size_t DEPTH_SHIFT = TILE_GRID_OFFSET * GRID_SIZE;

constexpr size_t HASH_WIDTH = 30;
constexpr uint32_t HASH_MASK = (1 << HASH_WIDTH) - 1;

// The board state is stored in the lower (TILE_GRID_OFFSET * GRID_SIZE) bits.
constexpr grid_t GRID_MASK = (1ul << (TILE_GRID_OFFSET * GRID_SIZE)) - 1;
constexpr grid_t DEPTH_MASK = ~GRID_MASK;

// --------------------------------------------------------------------------
// Inline Helper Functions
// --------------------------------------------------------------------------

inline tile_t getTile(grid_t grid, tile_index_t idx)
{ return (grid >> (idx * TILE_GRID_OFFSET)) & TIGHT_TILE_MASK; }

inline grid_t setTile(grid_t grid, tile_index_t idx, tile_t value)
{
    grid &= ~(static_cast<grid_t>(LOOSE_TILE_MASK) << (idx * TILE_GRID_OFFSET));
    grid |= (static_cast<grid_t>(value) & LOOSE_TILE_MASK) << (idx * TILE_GRID_OFFSET);
    return grid;
}

// --------------------------------------------------------------------------
// Print Functions (for debugging)
// --------------------------------------------------------------------------

#ifdef BUILD_TESTS
std::string printFromGridT(grid_t grid)
{
    std::stringstream output;
    output << "Depth: " << (grid >> DEPTH_SHIFT) << "\nGrid:\n\t";
    for (size_t i = 0; i < GRID_SIZE; i++) {
        output << ((grid >> (i * TILE_GRID_OFFSET)) & LOOSE_TILE_MASK) << " ";
        if ((i + 1) % GRID_SIDE_SIZE == 0)
            output << "\n\t";
    }
    return output.str();
}

std::string printFromRawHashT(raw_hash_t hash)
{
    std::stringstream output;
    output << "Hash:\n\t";
    for (size_t i = 0; i < GRID_SIZE; i++) {
        output << hash[i] << " ";
        if ((i + 1) % GRID_SIDE_SIZE == 0)
            output << "\n\t";
    }
    return output.str();
}
#endif

// --------------------------------------------------------------------------
// Transformer Classes (for symmetry handling)
// --------------------------------------------------------------------------

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
        // clang-format off
        if (static_cast<TransformOperations>(t) & H)
        {
            transformed_key =
                ((transformed_key & 0b11111'11111'11111'00000'00000'00000'00000'00000'00000)
                    >> (TILE_GRID_OFFSET * 6)) |
                (transformed_key & 0b00000'00000'00000'11111'11111'11111'00000'00000'00000) |
                ((transformed_key & 0b00000'00000'00000'00000'00000'00000'11111'11111'11111) << (TILE_GRID_OFFSET * 6));
        }
        if (static_cast<TransformOperations>(t) & V)
        {
            transformed_key =
                ((transformed_key & 0b11111'00000'00000'11111'00000'00000'11111'00000'00000)
                    >> (TILE_GRID_OFFSET * 2)) |
                (transformed_key & 0b00000'11111'00000'00000'11111'00000'00000'11111'00000) |
                ((transformed_key & 0b00000'00000'11111'00000'00000'11111'00000'00000'11111) << (TILE_GRID_OFFSET * 2));
        }
        if (static_cast<TransformOperations>(t) & T)
        {
            transformed_key =
                (transformed_key & 0b11111'00000'00000'00000'11111'00000'00000'00000'11111) |
                ((transformed_key & 0b00000'11111'00000'00000'00000'11111'00000'00000'00000)
                    >> (TILE_GRID_OFFSET * 2)) |
                ((transformed_key & 0b00000'00000'00000'11111'00000'00000'00000'11111'00000)
                    << (TILE_GRID_OFFSET * 2)) |
                ((transformed_key & 0b00000'00000'11111'00000'00000'00000'00000'00000'00000)
                    >> (TILE_GRID_OFFSET * 4)) |
                ((transformed_key & 0b00000'00000'00000'00000'00000'00000'11111'00000'00000) << (TILE_GRID_OFFSET * 4));
        }
        // clang-format on
        return transformed_key | depth_part;
    }

    static constexpr std::pair<hash_key_t, Transformation> canonical(hash_key_t key)
    {
        const tile_t first_corner = (key >> (TILE_GRID_OFFSET * 8)) & TIGHT_TILE_MASK;
        const tile_t second_corner = (key >> (TILE_GRID_OFFSET * 6)) & TIGHT_TILE_MASK;
        const tile_t third_corner = (key >> (TILE_GRID_OFFSET * 2)) & TIGHT_TILE_MASK;
        const tile_t fourth_corner = key & TIGHT_TILE_MASK;
        const tile_t min_corner = std::min({first_corner, second_corner, third_corner, fourth_corner});
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
        if (found == 1) [[likely]]
        {
            Transformation transposed =
                static_cast<Transformation>(static_cast<TransformOperations>(transformation) | T);
            hash_key_t transformation_key = transform(key, transformation);
            hash_key_t transposed_key = transform(transformation_key, Transformation::TRANSPOSE);
            if (transposed_key < transformation_key)
            {
                return {transposed_key, transposed};
            }
            return {transformation_key, transformation};
        }
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
            transformed_hash =
                {{
                     transformed_hash[6],
                     transformed_hash[7],
                     transformed_hash[8],
                     transformed_hash[3],
                     transformed_hash[4],
                     transformed_hash[5],
                     transformed_hash[0],
                     transformed_hash[1],
                     transformed_hash[2],
                 }};
        }
        if (static_cast<TransformOperations>(t) & V)
        {
            transformed_hash =
                {{
                     transformed_hash[2],
                     transformed_hash[1],
                     transformed_hash[0],
                     transformed_hash[5],
                     transformed_hash[4],
                     transformed_hash[3],
                     transformed_hash[8],
                     transformed_hash[7],
                     transformed_hash[6],
                 }};
        }
        if (static_cast<TransformOperations>(t) & T)
        {
            transformed_hash =
                {{
                     transformed_hash[0],
                     transformed_hash[3],
                     transformed_hash[6],
                     transformed_hash[1],
                     transformed_hash[4],
                     transformed_hash[7],
                     transformed_hash[2],
                     transformed_hash[5],
                     transformed_hash[8],
                 }};
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

// --------------------------------------------------------------------------
// Memoization Hash Lookup – using std::unordered_map. (For extra speed in a contest,
// you might use a flat-hash implementation.)
// --------------------------------------------------------------------------

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
            return EntryTransformer::reverseTransform(it->second, t);
        }
        return std::nullopt;
    }

    void store(hash_key_t key, raw_hash_t hash)
    {
        auto [transformed_key, t] = KeyTransformer::canonical(key);
        _hash_lookup[transformed_key] = EntryTransformer::transform(hash, t);
    }

    void clear()
    { _hash_lookup.clear(); }
};

// --------------------------------------------------------------------------
// Precomputed LUT for Capturing Combinations per Tile.
// Each tile index (0–8) uses a predefined required sum code and a shift multiplier.
// (For instance, tile 0 uses combination SUM_AB with a multiplier of 1, meaning that
// for capture it sums the two adjacent cells stored in the next 5‐bit slot.)
// --------------------------------------------------------------------------

struct CaptureLUTEntry
{
    sum_t required_sum_code;
    size_t tile_shift;
};

enum Sum : uint8_t
{
    AB = 0b1100,
    AC = 0b1010,
    AD = 0b1001,
    BC = 0b0110,
    BD = 0b0101,
    CD = 0b0011,
    ABC = 0b1110,
    ABD = 0b1101,
    ACD = 0b1011,
    BCD = 0b0111,
    ABCD = 0b1111,
};

// These values were derived from your debug output and final working version.
constexpr std::array<CaptureLUTEntry, GRID_SIZE> CAPTURE_LUT =
    {{
         {.required_sum_code = AB, .tile_shift = 1},   // Tile 0
         {.required_sum_code = ABC, .tile_shift = 0},  // Tile 1
         {.required_sum_code = AC, .tile_shift = 1},   // Tile 2
         {.required_sum_code = ACD, .tile_shift = 0},  // Tile 3
         {.required_sum_code = ABCD, .tile_shift = 1}, // Tile 4
         {.required_sum_code = ABD, .tile_shift = 2},  // Tile 5
         {.required_sum_code = AC, .tile_shift = 3},   // Tile 6
         {.required_sum_code = ABC, .tile_shift = 4},  // Tile 7
         {.required_sum_code = AB, .tile_shift = 5},   // Tile 8
     }};

// --------------------------------------------------------------------------
// Precomputed static masks for sum combinations
// --------------------------------------------------------------------------
constexpr grid_t MASK_OFFSET_A = LOOSE_TILE_MASK;
constexpr grid_t MASK_OFFSET_B = LOOSE_TILE_MASK << (TILE_GRID_OFFSET * 2);
constexpr grid_t MASK_OFFSET_C = LOOSE_TILE_MASK << (TILE_GRID_OFFSET * 4);
constexpr grid_t MASK_OFFSET_D = LOOSE_TILE_MASK << (TILE_GRID_OFFSET * 6);

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

// --------------------------------------------------------------------------
// Sum Combination LUT
// --------------------------------------------------------------------------
struct SumCombo
{
    sum_t sum_code;
    grid_t success_mask;
};

constexpr std::array<SumCombo, 11> SUM_COMBOS = {{
                                                     {.sum_code = AB, .success_mask = MASK_SUM_AB},
                                                     {.sum_code = AC, .success_mask = MASK_SUM_AC},
                                                     {.sum_code = AD, .success_mask = MASK_SUM_AD},
                                                     {.sum_code = BC, .success_mask = MASK_SUM_BC},
                                                     {.sum_code = BD, .success_mask = MASK_SUM_BD},
                                                     {.sum_code = CD, .success_mask = MASK_SUM_CD},
                                                     {.sum_code = ABC, .success_mask = MASK_SUM_ABC},
                                                     {.sum_code = ABD, .success_mask = MASK_SUM_ABD},
                                                     {.sum_code = ACD, .success_mask = MASK_SUM_ACD},
                                                     {.sum_code = BCD, .success_mask = MASK_SUM_BCD},
                                                     {.sum_code = ABCD, .success_mask = MASK_SUM_ABCD},
                                                 }};

// A mask used to check that the expected cells are present.
constexpr grid_t CHECK_PRESENCE_MASK = 1ul | (1ul << TILE_GRID_OFFSET) | (1ul << (TILE_GRID_OFFSET * 2)) |
                                       (1ul << (TILE_GRID_OFFSET * 3)) | (1ul << (TILE_GRID_OFFSET * 4)) |
                                       (1ul << (TILE_GRID_OFFSET * 5)) | (1ul << (TILE_GRID_OFFSET * 6)) |
                                       (1ul << (TILE_GRID_OFFSET * 7)) | (1ul << (TILE_GRID_OFFSET * 8));

// --------------------------------------------------------------------------
// (Optional) Statistics for testing/analysis (not essential for competition)
// --------------------------------------------------------------------------
#ifdef BUILD_TESTS
struct Statistics
{
    size_t end_states_count = 0;
    size_t states_stored_in_hash_map = 0;
    size_t states_retrieved_from_hash_map = 0;
    void printStatistics() const {
        std::cerr << "End states count: " << end_states_count << "\n";
        std::cerr << "States stored in hash map: " << states_stored_in_hash_map << "\n";
        std::cerr << "States retrieved from hash map: " << states_retrieved_from_hash_map << "\n";
    }
} statistics;
#endif

// --------------------------------------------------------------------------
// Class Position – implements state expansion using all improvements.
// --------------------------------------------------------------------------
class Position
{
  private:
    grid_t _grid;
    static RawHashLookup _hash_lookup;

    // NextPositions holds a small, fixed-size array of possible next state grids.
    struct NextPositions
    {
        std::array<grid_t, MAX_NEXT_POSITION_COUNT> positions{};
        size_t count = 0;

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

    // Optimized nextPositions() uses the precomputed LUT and dynamic sum arrays.
    [[nodiscard]] NextPositions nextPositions() const
    {
        NextPositions next_positions;
        if ((_grid & DEPTH_MASK) == 0)
        {
            return next_positions;
        }

        // Mask out the depth bits to work with the board only.
        const grid_t& board = _grid;
        const grid_t offset_a = board;
        const grid_t offset_b = board >> (TILE_GRID_OFFSET * 2);
        const grid_t offset_c = board >> (TILE_GRID_OFFSET * 4);
        const grid_t offset_d = board >> (TILE_GRID_OFFSET * 6);

        // Compute a presence mask for the board cells.
        const grid_t value_present = offset_a | (offset_a >> 1) | (offset_a >> 2);

        // Precompute dynamic sums for each sum combination:
        std::array<grid_t, 11> dynamic_sums{};
        dynamic_sums[0] = offset_a + offset_b;
        dynamic_sums[1] = offset_a + offset_c;
        dynamic_sums[2] = offset_a + offset_d;
        dynamic_sums[3] = offset_b + offset_c;
        dynamic_sums[4] = offset_b + offset_d;
        dynamic_sums[5] = offset_c + offset_d;
        dynamic_sums[6] = offset_a + offset_b + offset_c;
        dynamic_sums[7] = offset_a + offset_b + offset_d;
        dynamic_sums[8] = offset_a + offset_c + offset_d;
        dynamic_sums[9] = offset_b + offset_c + offset_d;
        dynamic_sums[10] = offset_a + offset_b + offset_c + offset_d;

        // Loop over every tile in the grid.
        for (tile_index_t i = 0; i < GRID_SIZE; ++i)
        {
            tile_t tile = getTile(_grid, i);
            if (tile != 0)
            {
                continue;
            } // Tile already occupied.
            bool found_capture = false;
            // Get the precomputed Capture LUT entry for this tile.
            auto lut_entry = CAPTURE_LUT[i];
            // Iterate over each capturing combination.
            for (size_t combo_idx = 0; combo_idx < SUM_COMBOS.size(); ++combo_idx)
            {
                const SumCombo &combo = SUM_COMBOS[combo_idx];
                // Check if the sum code combination is valid for this tile.
                if ((combo.sum_code & lut_entry.required_sum_code) != combo.sum_code)
                    continue;
                // Calculate the target mask; shift the success mask by the tile-specific offset.
                grid_t target_mask = combo.success_mask << (lut_entry.tile_shift * TILE_GRID_OFFSET);
                grid_t target_presence_mask = target_mask & CHECK_PRESENCE_MASK;
                if ((value_present & target_presence_mask) != target_presence_mask)
                    continue; // Not all required cells are non-empty.
                // Extract the dynamic sum for this capturing combination.
                tile_t sum_value =
                    (dynamic_sums[combo_idx] >> (lut_entry.tile_shift * TILE_GRID_OFFSET)) & LOOSE_TILE_MASK;
                if (sum_value > 6)
                    continue;
                // Valid capture move: remove the captured tiles and place a die showing the sum.
                grid_t new_grid = _grid & ~target_mask;
                new_grid = setTile(new_grid, i, sum_value);
                new_grid -= (1ul << DEPTH_SHIFT); // Decrement depth.
                found_capture = true;
                next_positions.emplace_back(new_grid);
            }
            if (!found_capture)
            {
                // No capture was possible: perform non-capturing move by placing a '1'.
                grid_t new_grid = setTile(_grid, i, 1);
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
    {}

    [[nodiscard]] raw_hash_t produceRawHash() const
    {
        raw_hash_t hash{};
        for (tile_index_t i = 0; i < GRID_SIZE; i++)
        {
            hash[i] = getTile(_grid, i);
        }
        return hash;
    }

    uint32_t outputHash()
    {
        raw_hash_t hash = calculateHash();
        uint32_t clean_hash = 0;
        for (tile_index_t i = 0; i < GRID_SIZE; i++)
        {
            clean_hash = clean_hash * 10 + hash[i];
        }
        return clean_hash & HASH_MASK;
    }

    raw_hash_t calculateHash()
    {
        if (auto cached = _hash_lookup.retrieve(_grid); cached.has_value())
        {
            return cached.value();
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
            Position next_position(next_positions.positions[i]);
            raw_hash_t next_hash = next_position.calculateHash();
            for (tile_index_t j = 0; j < GRID_SIZE; j++)
            {
                hash[j] += next_hash[j];
            }
        }
        _hash_lookup.store(_grid, hash);
        return hash;
    }
};

RawHashLookup Position::_hash_lookup;

// --------------------------------------------------------------------------
// getInitialPosition() reads input and builds the initial grid state.
// --------------------------------------------------------------------------
grid_t getInitialPosition()
{
    grid_t grid = 0;
    int value;
    std::cin >> value;
    std::cin.ignore();
    depth_t distance_from_depth = value;
    for (tile_index_t i = 0; i < GRID_SIZE; i++)
    {
        std::cin >> value;
        std::cin.ignore();
        grid = setTile(grid, i, value & TIGHT_TILE_MASK);
    }
    grid |= (static_cast<grid_t>(distance_from_depth) << DEPTH_SHIFT);
    return grid;
}

// --------------------------------------------------------------------------
// Main function
// --------------------------------------------------------------------------
#ifdef BUILD_TESTS
int testMain()
#else

int main()
#endif
{
    grid_t initial_grid = getInitialPosition();
    Position initial_position(initial_grid);
    uint32_t hash = initial_position.outputHash();
    std::cout << hash << std::endl;
#ifdef BUILD_TESTS
    statistics.printStatistics();
    Position::clearHashLookup();
#endif
    return 0;
}
