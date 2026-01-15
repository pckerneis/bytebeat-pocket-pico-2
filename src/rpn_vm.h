#pragma once
#include <stdint.h>
#include <stdbool.h>

#define TEXT_BUFFER_SIZE 256
#define MAX_TOKENS 256
#define RPN_STACK_SIZE 8
#define RPN_PROGRAM_SIZE 32

enum TokenType {
  TOK_T,
  TOK_NUM,
  TOK_OP
};

enum OpType {
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_MOD,
  OP_AND,
  OP_OR,
  OP_XOR,
  OP_NOT,
  OP_SHL,
  OP_SHR,
  OP_LT,
  OP_GT,
  OP_EQ,
  OP_LE,
  OP_GE,
  OP_NE,
  OP_LEFT_PAREN,
  OP_RIGHT_PAREN
};

enum CompileError {
  ERR_NONE = 0,
  ERR_PAREN,
  ERR_STACK,
  ERR_TOKEN,
  ERR_PROGRAM_TOO_LONG
};

struct Token {
  uint8_t type;
  uint32_t value;
};

enum RpnOpcode {
  RPN_PUSH_T,
  RPN_PUSH_NUM,
  RPN_ADD,
  RPN_SUB,
  RPN_MUL,
  RPN_DIV,
  RPN_MOD,
  RPN_AND,
  RPN_OR,
  RPN_XOR,
  RPN_NOT,
  RPN_NEG,
  RPN_SHL,
  RPN_SHR,
  RPN_LT,
  RPN_GT,
  RPN_EQ,
  RPN_LE,
  RPN_GE,
  RPN_NE
};

struct RpnInstruction {
  uint8_t opcode;
  uint32_t value;
};

// Global variables
extern struct Token expr[MAX_TOKENS];
extern volatile enum CompileError compileError;
extern uint8_t expr_len;
extern char textBuffer[TEXT_BUFFER_SIZE];
extern uint8_t text_len;
extern uint8_t cursor;
extern bool needsRecompile;
extern bool needsResetT;

// Function prototypes
uint8_t compileToRPN(struct RpnInstruction *dst);
uint32_t executeRPN(uint32_t tval, const struct RpnInstruction* program, uint8_t program_len);
uint8_t getPrecedence(uint8_t opcode);
bool isHexDigit(char c);
