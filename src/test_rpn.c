#include "rpn_vm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

void test_rpn_vm() {
    printf("Testing RPN VM...\n");
    
    // Temporary buffer for testing
    struct RpnInstruction testProgram[RPN_PROGRAM_SIZE];
    
    // Test 1: Simple arithmetic
    strcpy(textBuffer, "1+2");
    text_len = strlen(textBuffer);
    uint8_t len = compileToRPN(testProgram);
    assert(compileError == ERR_NONE);
    assert(len > 0);
    
    uint32_t result = executeRPN(0, testProgram, len);
    assert(result == 3);
    printf("✓ Test 1 passed: 1+2 = %lu\n", result);
    
    // Test 2: Variable t
    strcpy(textBuffer, "t*2");
    text_len = strlen(textBuffer);
    len = compileToRPN(testProgram);
    assert(compileError == ERR_NONE);
    
    result = executeRPN(5, testProgram, len);
    assert(result == 10);
    printf("✓ Test 2 passed: t*2 with t=5 = %lu\n", result);
    
    // Test 3: Bitwise operations
    strcpy(textBuffer, "0b1010&0b1100");
    text_len = strlen(textBuffer);
    len = compileToRPN(testProgram);
    assert(compileError == ERR_NONE);
    
    result = executeRPN(0, testProgram, len);
    assert(result == 8); // 0b1000
    printf("✓ Test 3 passed: 0b1010&0b1100 = %lu\n", result);
    
    // Test 4: Hex numbers
    strcpy(textBuffer, "0xFF+0x01");
    text_len = strlen(textBuffer);
    len = compileToRPN(testProgram);
    assert(compileError == ERR_NONE);
    
    result = executeRPN(0, testProgram, len);
    assert(result == 256);
    printf("✓ Test 4 passed: 0xFF+0x01 = %lu\n", result);
    
    // Test 5: Complex expression (classic bytebeat)
    strcpy(textBuffer, "t*(42&t>>10)");
    text_len = strlen(textBuffer);
    len = compileToRPN(testProgram);
    assert(compileError == ERR_NONE);
    
    // Test with specific t values
    result = executeRPN(1000, testProgram, len);
    printf("✓ Test 5 passed: t*(42&t>>10) with t=1000 = %lu\n", result);
    
    // Test 6: Operator precedence
    strcpy(textBuffer, "2+3*4");
    text_len = strlen(textBuffer);
    len = compileToRPN(testProgram);
    assert(compileError == ERR_NONE);
    
    result = executeRPN(0, testProgram, len);
    assert(result == 14); // 2 + (3*4)
    printf("✓ Test 6 passed: 2+3*4 = %lu\n", result);
    
    // Test 7: Parentheses
    strcpy(textBuffer, "(2+3)*4");
    text_len = strlen(textBuffer);
    len = compileToRPN(testProgram);
    assert(compileError == ERR_NONE);
    
    result = executeRPN(0, testProgram, len);
    assert(result == 20); // (2+3)*4
    printf("✓ Test 7 passed: (2+3)*4 = %lu\n", result);
    
    // Test 8: Error handling - unary plus (should compile to empty program)
    strcpy(textBuffer, "+");
    text_len = strlen(textBuffer);
    len = compileToRPN(testProgram);
    // Unary + is skipped, leaving no operand, which is an error
    assert(compileError != ERR_NONE);
    printf("✓ Test 8 passed: Error handling for unary plus with no operand\n");
    
    // Test 9: Error handling - mismatched parentheses
    strcpy(textBuffer, "(1+2");
    text_len = strlen(textBuffer);
    len = compileToRPN(testProgram);
    assert(compileError == ERR_PAREN);
    printf("✓ Test 9 passed: Error handling for mismatched parentheses\n");
    
    printf("All RPN VM tests passed! ✓\n\n");
}
