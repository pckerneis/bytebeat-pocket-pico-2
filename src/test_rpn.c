#include "test_rpn.h"
#include "rpn_vm.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Enable or disable tests
#ifndef RPN_TESTS_ENABLED
#define RPN_TESTS_ENABLED 1
#endif

// Function pointer type for C test expressions
typedef uint32_t (*TestFunction)(uint32_t t);

// Test case structure
typedef struct {
    const char* name;
    const char* expression;
    TestFunction c_function;
} TestCase;

// C implementations of test expressions for comparison
static uint32_t test_expr_1(uint32_t t) {
    return t*(0xdeadbeef>>(t>>11)&15)/2|t>>3|t>>(t>>10);
}

static uint32_t test_expr_2(uint32_t t) {
    return t*((0xdeadbeef>>(15&t>>10)*4&15));
}

static uint32_t test_expr_3(uint32_t t) {
    return t*t>>8;
}

static uint32_t test_expr_4(uint32_t t) {
    return (t>>10&42)*t;
}

static uint32_t test_expr_5(uint32_t t) {
    return t*(t>>8|t>>9);
}

static uint32_t test_expr_6(uint32_t t) {
    return (t>>6|t|t>>(t>>16))*10+((t>>11)&7);
}

static uint32_t test_expr_7(uint32_t t) {
    return t|(t>>9|t>>7);
}

static uint32_t test_expr_8(uint32_t t) {
    return t*5&t>>7|t*3&t>>10;
}

// Test cases array
static TestCase testCases[] = {
    {
        "Complex expression 1",
        "t*(0xdeadbeef>>(t>>11)&15)/2|t>>3|t>>(t>>10)",
        test_expr_1
    },
    {
        "Complex expression 2",
        "t*((0xdeadbeef>>(15&t>>10)*4&15))",
        test_expr_2
    },
    {
        "Simple t*t>>8",
        "t*t>>8",
        test_expr_3
    },
    {
        "Bitwise operations",
        "(t>>10&42)*t",
        test_expr_4
    },
    {
        "OR operations",
        "t*(t>>8|t>>9)",
        test_expr_5
    },
    {
        "Complex shifts",
        "(t>>6|t|t>>(t>>16))*10+((t>>11)&7)",
        test_expr_6
    },
    {
        "Multiple OR",
        "t|(t>>9|t>>7)",
        test_expr_7
    },
    {
        "Mask operations",
        "t*5&t>>7|t*3&t>>10",
        test_expr_8
    }
};

#define NUM_TEST_CASES (sizeof(testCases) / sizeof(TestCase))

// Run a single test case
static bool runTestCase(TestCase* test, uint32_t startT, uint32_t samples, bool verbose) {
    printf("\n=== Testing: %s ===\n", test->name);
    printf("Expression: %s\n", test->expression);

    // Compile expression to RPN
    strncpy(textBuffer, test->expression, TEXT_BUFFER_SIZE - 1);
    textBuffer[TEXT_BUFFER_SIZE - 1] = '\0';
    text_len = strlen(textBuffer);

    struct RpnInstruction program[RPN_PROGRAM_SIZE];
    uint8_t program_len = compileToRPN(program);

    if (compileError != ERR_NONE) {
        printf("COMPILE ERROR: %d\n", compileError);
        return false;
    }

    printf("Compiled to %d RPN instructions\n", program_len);

    // Test sample by sample
    uint32_t firstDiffT = 0;
    uint32_t diffCount = 0;
    bool hasDifferences = false;

    for (uint32_t t = startT; t < startT + samples; t++) {
        uint32_t c_result = test->c_function(t);
        uint32_t vm_result = executeRPN(t, program, program_len);

        // Compare only the bottom 8 bits (audio output)
        uint8_t c_byte = (uint8_t)(c_result & 0xFF);
        uint8_t vm_byte = (uint8_t)(vm_result & 0xFF);

        if (c_byte != vm_byte) {
            if (!hasDifferences) {
                firstDiffT = t;
                hasDifferences = true;
            }
            diffCount++;

            if (verbose && diffCount <= 10) {
                printf("  DIFF at t=%lu: C=0x%08lX (%u) VM=0x%08lX (%u) [byte: C=%u VM=%u]\n",
                       (unsigned long)t,
                       (unsigned long)c_result, (unsigned int)c_byte,
                       (unsigned long)vm_result, (unsigned int)vm_byte,
                       (unsigned int)c_byte, (unsigned int)vm_byte);
            }
        }
    }

    if (hasDifferences) {
        printf("FAILED: %lu/%lu samples differ (%.2f%%)\n",
               (unsigned long)diffCount,
               (unsigned long)samples,
               100.0f * diffCount / samples);
        printf("First difference at t=%lu\n", (unsigned long)firstDiffT);
        return false;
    } else {
        printf("PASSED: All %lu samples match!\n", (unsigned long)samples);
        return true;
    }
}

// Run all tests
void runAllTests(uint32_t startT, uint32_t samples, bool verbose) {
    printf("\n");
    printf("=====================================\n");
    printf("  RPN VM Unit Tests\n");
    printf("=====================================\n");
    printf("Testing %lu samples starting from t=%lu\n",
           (unsigned long)samples, (unsigned long)startT);
    printf("Number of test cases: %d\n", (int)NUM_TEST_CASES);

    int passed = 0;
    int failed = 0;

    for (int i = 0; i < (int)NUM_TEST_CASES; i++) {
        if (runTestCase(&testCases[i], startT, samples, verbose)) {
            passed++;
        } else {
            failed++;
        }
    }

    printf("\n");
    printf("=====================================\n");
    printf("  Test Summary\n");
    printf("=====================================\n");
    printf("Passed: %d/%d\n", passed, (int)NUM_TEST_CASES);
    printf("Failed: %d/%d\n", failed, (int)NUM_TEST_CASES);

    if (failed == 0) {
        printf("\nALL TESTS PASSED!\n");
    } else {
        printf("\nSOME TESTS FAILED!\n");
    }
    printf("=====================================\n\n");
}

// Run a specific test by index
void runSingleTest(int testIndex, uint32_t startT, uint32_t samples, bool verbose) {
    if (testIndex < 0 || testIndex >= (int)NUM_TEST_CASES) {
        printf("Invalid test index: %d (valid range: 0-%d)\n", testIndex, (int)NUM_TEST_CASES - 1);
        return;
    }

    runTestCase(&testCases[testIndex], startT, samples, verbose);
}

// List all available tests
void listTests(void) {
    printf("\nAvailable test cases:\n");
    for (int i = 0; i < (int)NUM_TEST_CASES; i++) {
        printf("  [%d] %s\n      %s\n", i, testCases[i].name, testCases[i].expression);
    }
    printf("\n");
}

// Main test entry point called from main()
void test_rpn_vm(void) {
#if RPN_TESTS_ENABLED
    printf("\n*** Running RPN VM Unit Tests ***\n");

    // Run quick smoke test (1000 samples)
    runAllTests(0, 1000, false);

    // Optionally run longer test if you want more thorough testing
    // runAllTests(0, 100000, false);
#else
    printf("RPN VM tests disabled (RPN_TESTS_ENABLED=0)\n");
#endif
}
