#pragma once
#include <stdint.h>
#include <stdbool.h>

/**
 * Main test entry point - called from main()
 * Runs a smoke test on all test cases
 */
void test_rpn_vm(void);

/**
 * Run all RPN VM unit tests
 *
 * @param startT Starting t value for testing
 * @param samples Number of samples to test
 * @param verbose If true, print detailed diff information
 */
void runAllTests(uint32_t startT, uint32_t samples, bool verbose);

/**
 * Run a specific test case by index
 *
 * @param testIndex Index of test to run (0-based)
 * @param startT Starting t value for testing
 * @param samples Number of samples to test
 * @param verbose If true, print detailed diff information
 */
void runSingleTest(int testIndex, uint32_t startT, uint32_t samples, bool verbose);

/**
 * List all available test cases
 */
void listTests(void);
