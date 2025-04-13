#include <gtest/gtest.h>
#include <string>

#include "../src/main.cpp" // Include the main program file

uint64_t initToKey(uint32_t init_position) {
    uint64_t key = 0;
    for (int i = 0; i < 9; i++) {
        uint8_t tile = init_position % 10;
        key = (key << 5) | tile;
        init_position /= 10;
    }
    return key;
}

std::array<uint32_t, 9> initToRawHash(uint32_t init_position) {
    std::array<uint32_t, 9> raw_hash{};
    for (int i = 0; i < 9; i++) {
        uint8_t tile = init_position % 10;
        raw_hash[i] = tile;
        init_position /= 10;
    }
    return raw_hash;
}

struct TestParams
{
    depth_t depth;
    uint32_t init_position;
    Transformer::Transformation transformation;
    uint32_t expected_output;
};

class KeyTransformations : public ::testing::TestWithParam<TestParams>
{};

// Test cases
INSTANTIATE_TEST_SUITE_P(
    MyParamGroup,
    KeyTransformations,
    ::testing::Values(
        // init_hash, expected_output
        TestParams{17, 123456789, Transformer::Transformation::ID, 123456789},
        TestParams{17, 123456789, Transformer::Transformation::ROT_90, 369258147},
        TestParams{17, 123456789, Transformer::Transformation::ROT_180, 987654321},
        TestParams{17, 123456789, Transformer::Transformation::ROT_270, 741852963},
        TestParams{17, 123456789, Transformer::Transformation::H_FLIP, 789456123},
        TestParams{17, 123456789, Transformer::Transformation::V_FLIP, 321654987},
        TestParams{17, 123456789, Transformer::Transformation::TRANSPOSE, 147258369},
        TestParams{17, 123456789, Transformer::Transformation::ANTI, 963852741}
    )
);

TEST_P(KeyTransformations, BasicCase) {
    /* =============== */
    /* Retrieve Params */
    /* =============== */
    const TestParams& params = GetParam();
    grid_t depth = static_cast<grid_t>(params.depth) << DEPTH_SHIFT;

    uint64_t key = initToKey(params.init_position) | depth;
    uint64_t expected_key = initToKey(params.expected_output) | depth;

    uint64_t transformed_key = KeyTransformer::transform(key, params.transformation);
    EXPECT_EQ(expected_key, transformed_key) << "Expected: " << std::endl
                                             << printFromGridT(expected_key) << "Got: " << std::endl
                                             << printFromGridT(transformed_key);

    std::array<uint32_t, 9> raw_hash = initToRawHash(params.init_position);
    std::array<uint32_t, 9> expected_raw_hash = initToRawHash(params.expected_output);

    std::array<uint32_t, 9> transformed_raw_hash = EntryTransformer::transform(raw_hash, params.transformation);
    EXPECT_EQ(expected_raw_hash, transformed_raw_hash) << "Expected: " << std::endl
                                                       << printFromRawHashT(expected_raw_hash) << "Got: " << std::endl
                                                       << printFromRawHashT(transformed_raw_hash);
}
