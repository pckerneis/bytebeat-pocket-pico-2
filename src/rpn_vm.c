#include "rpn_vm.h"
#include <string.h>
#include <stdio.h>

#define MAX_OP_STACK_SIZE 64
#define OP_PAREN_OPEN  255
#define OP_PAREN_CLOSE 254

// Global variables
struct Token expr[MAX_TOKENS];
volatile enum CompileError compileError = ERR_NONE;
uint8_t expr_len = 0;
char textBuffer[TEXT_BUFFER_SIZE];
uint8_t text_len = 0;
uint8_t cursor = 0;
bool needsRecompile = false;
bool needsResetT = false;

// Helper function to check if character is a hex digit
bool isHexDigit(char c) {
  return (c >= '0' && c <= '9') || 
         (c >= 'a' && c <= 'f') || 
         (c >= 'A' && c <= 'F');
}

uint8_t getPrecedence(uint8_t opcode) {
  switch (opcode) {
    case RPN_NOT:
    case RPN_NEG: return 7;          // ~ and unary -

    case RPN_MUL:
    case RPN_DIV:
    case RPN_MOD: return 6;          // * / %

    case RPN_ADD:
    case RPN_SUB: return 5;          // + -

    case RPN_SHL:
    case RPN_SHR: return 4;          // << >>

    case RPN_LT:
    case RPN_GT:
    case RPN_LE:
    case RPN_GE:  return 3;          // < > <= >=

    case RPN_EQ:
    case RPN_NE:  return 2;          // == !=

    case RPN_AND: return 1;          // &
    case RPN_XOR: return 0;          // ^
    case RPN_OR:  return 0;          // |

    default: return 0;
  }
}

// Compile text to RPN using shunting-yard algorithm
uint8_t compileToRPN(struct RpnInstruction *dst) {
  compileError = ERR_NONE;
  uint8_t rpnProgramLen = 0;
  
  // Operator stack for shunting-yard
  uint8_t opStack[MAX_OP_STACK_SIZE];
  uint8_t opStackTop = 0;
  uint8_t numParentheses = 0;

  bool expectOperand = true;
  
  uint8_t i = 0;

  while (textBuffer[i] != '\0') {
    char c = textBuffer[i];

    if (rpnProgramLen >= RPN_PROGRAM_SIZE || i >= TEXT_BUFFER_SIZE) {
      compileError = ERR_PROGRAM_TOO_LONG;
      return 0;
    }
    
    // Skip whitespace
    if (c == ' ') {
      i++;
      continue;
    }
    
    // Numbers
    if (c >= '0' && c <= '9') {
      if (!expectOperand) {
        compileError = ERR_TOKEN;
        return 0;
      }

      uint32_t num = 0;
      
      // Check for hex or binary prefix
      if (c == '0' && i + 2 < TEXT_BUFFER_SIZE) {
        char next = textBuffer[i + 1];
        if (next == 'x' || next == 'X') {
          // Hex number: 0x...
          i += 2; // skip '0x'
          if (i >= TEXT_BUFFER_SIZE || !isHexDigit(textBuffer[i])) {
            compileError = ERR_TOKEN;
            return 0;
          }
          while (i < TEXT_BUFFER_SIZE) {
            char hexChar = textBuffer[i];
            uint8_t digit;
            if (hexChar >= '0' && hexChar <= '9') {
              digit = hexChar - '0';
            } else if (hexChar >= 'a' && hexChar <= 'f') {
              digit = hexChar - 'a' + 10;
            } else if (hexChar >= 'A' && hexChar <= 'F') {
              digit = hexChar - 'A' + 10;
            } else {
              break; // not a hex digit
            }
            num = (num << 4) | digit;
            i++;
            c = textBuffer[i];
          }
        } else if (next == 'b' || next == 'B') {
          // Binary number: 0b...
          i += 2; // skip '0b'
          if (i >= TEXT_BUFFER_SIZE || (textBuffer[i] != '0' && textBuffer[i] != '1')) {
            compileError = ERR_TOKEN;
            return 0;
          }
          while (i < TEXT_BUFFER_SIZE && (textBuffer[i] == '0' || textBuffer[i] == '1')) {
            num = (num << 1) | (textBuffer[i] - '0');
            i++;
            c = textBuffer[i];
          }
        } else {
          // Decimal number
          while (c >= '0' && c <= '9' && i < TEXT_BUFFER_SIZE) {
            num = num * 10 + (c - '0');
            i++;
            c = textBuffer[i];
          }
        }
      } else {
        // Decimal number (no leading 0)
        while (c >= '0' && c <= '9' && i < TEXT_BUFFER_SIZE) {
          num = num * 10 + (c - '0');
          i++;
          c = textBuffer[i];
        }
      }

      if (rpnProgramLen >= RPN_PROGRAM_SIZE) {
        compileError = ERR_PROGRAM_TOO_LONG;
        return 0;
      }
      
      dst[rpnProgramLen].opcode = RPN_PUSH_NUM;
      dst[rpnProgramLen].value = num;
      rpnProgramLen++;
      expectOperand = false;
      continue;
    }
    
    // Variable t
    if (c == 't') {
      if (!expectOperand) {
        compileError = ERR_TOKEN;
        return 0;
      }

      if (rpnProgramLen >= RPN_PROGRAM_SIZE) {
        compileError = ERR_PROGRAM_TOO_LONG;
        return 0;
      }

      dst[rpnProgramLen].opcode = RPN_PUSH_T;
      dst[rpnProgramLen].value = 0;
      rpnProgramLen++;
      expectOperand = false;
      i++;
      continue;
    }
    
    // Operators and parentheses
    uint8_t precedence = 0;
    uint8_t opcode = 0;
    bool rightAssoc = false;
    bool isBinaryOp = false;
    
    switch (c) {
      case '(':
        if (!expectOperand) {
          compileError = ERR_TOKEN;
          return 0;
        }
        opcode = OP_PAREN_OPEN;
        numParentheses++;
        expectOperand = true;
        break;
      case ')':
        if (expectOperand) {
          compileError = ERR_PAREN;
          return 0;
        }
        if (numParentheses <= 0) {
          compileError = ERR_TOKEN;
          return 0;
        }
        precedence = 0;
        opcode = OP_PAREN_CLOSE;
        numParentheses--;
        expectOperand = false;
        break;
      case '~':
        if (!expectOperand) {
          compileError = ERR_TOKEN;
          return 0;
        }
        precedence = getPrecedence(RPN_NOT);
        opcode = RPN_NOT;
        rightAssoc = true;
        break;
      case '*': case '/': case '%': precedence = getPrecedence(RPN_MUL); opcode = (c == '*') ? RPN_MUL : (c == '/') ? RPN_DIV : RPN_MOD; isBinaryOp = true; break;
      case '+': 
        if (expectOperand) {
          // Unary plus - just skip it
          i++;
          continue;
        }
        precedence = getPrecedence(RPN_ADD);
        opcode = RPN_ADD;
        isBinaryOp = true;
        break;
      case '-':
        if (expectOperand) {
          // Unary minus
          precedence = getPrecedence(RPN_NEG);
          opcode = RPN_NEG;
          rightAssoc = true;
          break;
        }
        precedence = getPrecedence(RPN_SUB);
        opcode = RPN_SUB;
        isBinaryOp = true;
        break;
      case '&': precedence = getPrecedence(RPN_AND); opcode = RPN_AND; isBinaryOp = true; break;
      case '|': precedence = getPrecedence(RPN_OR); opcode = RPN_OR; isBinaryOp = true; break;
      case '^': precedence = getPrecedence(RPN_XOR); opcode = RPN_XOR; isBinaryOp = true; break;
      case '<': 
        if (textBuffer[i+1] == '<') { precedence = getPrecedence(RPN_SHL); opcode = RPN_SHL; i++; }
        else if (textBuffer[i+1] == '=') { precedence = getPrecedence(RPN_LE); opcode = RPN_LE; i++; }
        else { precedence = getPrecedence(RPN_LT); opcode = RPN_LT; }
        isBinaryOp = true;
        break;
      case '>': 
        if (textBuffer[i+1] == '=') { precedence = getPrecedence(RPN_GE); opcode = RPN_GE; i++; }
        else if (textBuffer[i+1] == '>') { precedence = getPrecedence(RPN_SHR); opcode = RPN_SHR; i++; }
        else { precedence = getPrecedence(RPN_GT); opcode = RPN_GT; }
        isBinaryOp = true;
        break;
      case '=': precedence = getPrecedence(RPN_EQ); opcode = RPN_EQ; isBinaryOp = true; break;
      default: {
        compileError = ERR_TOKEN;
        return 0;
      }
    }

    if (isBinaryOp) {
      if (expectOperand) {
        compileError = ERR_TOKEN;
        return 0;
      }
      expectOperand = true;
    }
    
    if (opcode == OP_PAREN_OPEN) { // '('
      if (opStackTop >= MAX_OP_STACK_SIZE) {
        compileError = ERR_PROGRAM_TOO_LONG;
        return 0;
      }
      opStack[opStackTop++] = opcode;
      i++;
    } else if (opcode == OP_PAREN_CLOSE) { // ')'
      // Pop until '('
      while (opStackTop > 0 && opStack[opStackTop-1] != OP_PAREN_OPEN) {
        if (rpnProgramLen >= RPN_PROGRAM_SIZE) {
          compileError = ERR_PROGRAM_TOO_LONG;
          return 0;
        }

        dst[rpnProgramLen].opcode = opStack[--opStackTop];
        dst[rpnProgramLen].value = 0;
        rpnProgramLen++;
        }
      if (opStackTop > 0) opStackTop--; // remove '('
      i++;
      } else {
      // Pop higher precedence operators
      while (opStackTop > 0 && opStack[opStackTop-1] != OP_PAREN_OPEN && 
             (!rightAssoc && precedence <= getPrecedence(opStack[opStackTop-1]) ||
              rightAssoc && precedence < getPrecedence(opStack[opStackTop-1]))) {
        if (rpnProgramLen >= RPN_PROGRAM_SIZE) {
          compileError = ERR_PROGRAM_TOO_LONG;
          return 0;
        }
        dst[rpnProgramLen].opcode = opStack[--opStackTop];
        dst[rpnProgramLen].value = 0;
        rpnProgramLen++;
      }

      if (opStackTop >= MAX_OP_STACK_SIZE) {
        compileError = ERR_PROGRAM_TOO_LONG;
        return 0;
      }

      opStack[opStackTop++] = opcode;
      i++;
    }
  }

  // Missing operand
  if (expectOperand) {
    compileError = ERR_TOKEN;
    return 0;
  }

  // Missing closing parenthesis
  if (numParentheses > 0) {
    compileError = ERR_PAREN;
    return 0;
  }
  
  // Pop remaining operators
  while (opStackTop > 0) {
    if (rpnProgramLen >= RPN_PROGRAM_SIZE) {
      compileError = ERR_PROGRAM_TOO_LONG;
      return 0;
    }

    dst[rpnProgramLen].opcode = opStack[--opStackTop];
    dst[rpnProgramLen].value = 0;
    rpnProgramLen++;
  }
  
  return rpnProgramLen;
}

// Execute RPN program
uint32_t executeRPN(uint32_t tval, const struct RpnInstruction* program, uint8_t program_len) {
  uint32_t stack[RPN_STACK_SIZE];
  uint8_t stackTop = 0;
  
  for (uint8_t pc = 0; pc < program_len; pc++) {
    uint8_t opcode = program[pc].opcode;
    
    switch (opcode) {
      case RPN_PUSH_T:
        if (stackTop < RPN_STACK_SIZE) stack[stackTop++] = tval;
        break;
        
      case RPN_PUSH_NUM:
        if (stackTop < RPN_STACK_SIZE) stack[stackTop++] = program[pc].value;
        break;
        
      case RPN_ADD:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a + b;
        }
        break;
        
      case RPN_SUB:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a - b;
        }
        break;
        
      case RPN_MUL:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a * b;
        }
        break;
        
      case RPN_DIV:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = b ? a / b : 0;
        }
        break;
        
      case RPN_MOD:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = b ? a % b : 0;
        }
        break;
        
      case RPN_AND:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a & b;
        }
        break;
        
      case RPN_OR:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a | b;
        }
        break;
        
      case RPN_XOR:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a ^ b;
        }
        break;
        
      case RPN_NOT:
        if (stackTop >= 1) {
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = ~a;
        }
        break;
        
      case RPN_NEG:
        if (stackTop >= 1) {
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = (uint32_t)(-(int32_t)a);
        }
        break;
        
      case RPN_SHL:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a << b;
        }
        break;

      case RPN_SHR:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a >> b;
        }
        break;
        
      case RPN_LT:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a < b;
        }
        break;
        
      case RPN_GT:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a > b;
        }
        break;
        
      case RPN_EQ:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a == b;
        }
        break;
        
      case RPN_LE:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a <= b;
        }
        break;
        
      case RPN_GE:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a >= b;
        }
        break;
        
      case RPN_NE:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop];
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a != b;
        }
        break;
    }
  }
  
  return stackTop > 0 ? stack[stackTop - 1] : 0;
}
