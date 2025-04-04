#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

int testMain(); // Forward declaration of the main function

struct TestParams
{
    size_t depth;
    uint32_t init_hash;
    uint32_t expected_output;
};

class MainIntegrationTest : public ::testing::TestWithParam<TestParams>
{};

// Test cases
INSTANTIATE_TEST_SUITE_P(
    MyParamGroup,
    MainIntegrationTest,
    ::testing::Values(
        // depth, init_hash, expected_output
        TestParams{20, 60254928, 322444322},  // Test 1;  end states: 2
        TestParams{20, 506482828, 951223336}, // Test 2;  end states: 6
        TestParams{1, 555038319, 36379286},   // Test 3;  end states: 2
        TestParams{1, 616134381, 264239762},  // Test 4;  end states: 11
        TestParams{8, 606033379, 76092874},   // Test 5;  end states: 1484
        TestParams{24, 300394867, 661168294}, // Test 6;  end states: 418440394
        TestParams{36, 604235164, 350917228}, // Test 7;  end states: 1014562252076
        TestParams{32, 86871, 999653138},     // Test 8;  end states: 104530503002231
        TestParams{40, 4056899, 521112022},   // Test 9;  end states: 946763082877
        TestParams{40, 54062794, 667094338},  // Test 10; end states: 559238314648167
        TestParams{20, 51033165, 738691369},  // Test 11; end states: 4017226136890
        TestParams{20, 100384866, 808014757}  // Test 12; end states: 950995003182
    )
);

TEST_P(MainIntegrationTest, BasicCase) {
    /* =============== */
    /* Retrieve Params */
    /* =============== */
    const TestParams& params = GetParam();

    // Process the hash to be passed to the program
    std::ostringstream hash_digits;
    hash_digits << std::setw(9) << std::setfill('0') << params.init_hash;
    std::string hash_str = hash_digits.str(); // e.g. "060254928"

    std::stringstream input_stream;
    input_stream << params.depth << "\n";
    for (char c : hash_str) {
        input_stream << c << "\n";
    }

    /* ============ */
    /* Redirect I/O */
    /* ============ */
    std::stringstream output_stream;

    std::streambuf* cin_old_buf = std::cin.rdbuf();
    std::streambuf* cout_old_buf = std::cout.rdbuf();

    std::cin.rdbuf(input_stream.rdbuf());
    std::cout.rdbuf(output_stream.rdbuf());

    /* =============== */
    /* Run the program */
    /* =============== */
    testMain();

    /* =========== */
    /* Restore I/O */
    /* =========== */
    std::cin.rdbuf(cin_old_buf);
    std::cout.rdbuf(cout_old_buf);

    /* ================ */
    /* Check the output */
    /* ================ */
    EXPECT_EQ(params.expected_output, std::stoi(output_stream.str()))
        << "Program output did not match the expected result.";
}
