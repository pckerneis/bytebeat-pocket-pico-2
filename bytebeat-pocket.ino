/*
# Index map reminder:
Row 0:  0   1   2   3   4
Row 1:  5   6   7   8   9
Row 2: 10  11  12  13  14
Row 3: 15  16  17  18  19
*/

#include <U8g2lib.h>
#include <Arduino.h>
#include <EEPROM.h>

#define KEY_FN1 8   // row 1, col 3
#define KEY_FN2 9   // row 2, col 3
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define TEXT_BUFFER_SIZE 256
#define MAX_TOKENS 256
#define FN_HOLD_THRESHOLD 300  // ms
#define PRESET_COUNT 3
#define SLOT_ADDR(i) (i * TEXT_BUFFER_SIZE)

U8G2_SSD1306_128X64_NONAME_1_HW_I2C display(
  U8G2_R0,
  /* reset = */ U8X8_PIN_NONE
);

// ===================== TOKENS =====================
enum TokenType {
  TOK_T,
  TOK_NUM,
  TOK_OP
};

enum OpType {
  // arithmetic
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_MOD,

  // bitwise
  OP_AND,
  OP_OR,
  OP_XOR,
  OP_NOT,

  // shifts
  OP_SHL,
  OP_SHR,

  // comparisons
  OP_LT,
  OP_GT,
  OP_EQ,
  OP_LE,
  OP_GE,
  OP_NE,
  
  // parentheses
  OP_LEFT_PAREN,
  OP_RIGHT_PAREN
};

enum CompileError : uint8_t {
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

enum KeyMode {
  MODE_BASE,
  MODE_FN1,
  MODE_FN2,
  MODE_MEM
};

enum Action {
  ACT_NONE,

  // navigation
  ACT_LEFT,
  ACT_RIGHT,

  // editing
  ACT_DEL,
  ACT_ENTER,

  // digits
  ACT_DIGIT_0,
  ACT_DIGIT_1,
  ACT_DIGIT_2,
  ACT_DIGIT_3,
  ACT_DIGIT_4,
  ACT_DIGIT_5,
  ACT_DIGIT_6,
  ACT_DIGIT_7,
  ACT_DIGIT_8,
  ACT_DIGIT_9,

  // operators / symbols
  ACT_ADD,
  ACT_SUB,
  ACT_MUL,
  ACT_DIV,
  ACT_MOD,
  ACT_AND,
  ACT_OR,
  ACT_XOR,
  ACT_NOT,
  ACT_GT,
  ACT_LT,
  ACT_EQ,
  ACT_DOT,
  ACT_QUOTE,
  ACT_COLON,
  ACT_SEMICOLON,
  ACT_QUESTION,
  ACT_0B,
  ACT_0X,

  // literals
  ACT_T,
  
  // parentheses
  ACT_LEFT_PAREN,
  ACT_RIGHT_PAREN,

  // alpha
  ACT_ALPHA_A,
  ACT_ALPHA_B,
  ACT_ALPHA_C,
  ACT_ALPHA_D,
  ACT_ALPHA_E,
  ACT_ALPHA_F,

  // Modes
  ACT_FN1,
  ACT_FN2,
  ACT_MEM,

  // presets
  ACT_PRESET_1,
  ACT_PRESET_2,
  ACT_PRESET_3,
  ACT_PRESET_4,
  ACT_PRESET_5,
  ACT_PRESET_6,
  ACT_PRESET_7,
  ACT_PRESET_8,
  ACT_PRESET_9,

  // preset navigation
  ACT_PRESET_DEC,
  ACT_PRESET_INC,

  ACT_SAVE
};

/*          
┌───┬───┬───┬───┬───┐
│ 7 │ 8 │ 9 │MEM│DEL│
├───┼───┼───┼───┼───┤
│ 4 │ 5 │ 6 │FN1│FN2│
├───┼───┼───┼───┼───┤
│ 1 │ 2 │ 3 │0b │0x │
├───┼───┼───┼───┼───┤
│ ← │ 0 │ → │ . │ ▸ │
└───┴───┴───┴───┴───┘
*/

const Action baseLayer[20] = {
  ACT_DIGIT_7, ACT_DIGIT_8, ACT_DIGIT_9, ACT_MEM, ACT_DEL,
  ACT_DIGIT_4, ACT_DIGIT_5, ACT_DIGIT_6, ACT_FN1, ACT_FN2,
  ACT_DIGIT_1, ACT_DIGIT_2, ACT_DIGIT_3, ACT_0B,  ACT_0X,
  ACT_LEFT,    ACT_DIGIT_0, ACT_RIGHT,   ACT_DOT, ACT_ENTER
};

/*
┌───┬───┬───┬───┬───┐
│ + │ - │ % │MEM│DEL│
├───┼───┼───┼───┼───┤
│ * │ / │ ~ │FN1│FN2│
├───┼───┼───┼───┼───┤
│ & │ | │ ^ │ < │ > │
├───┼───┼───┼───┼───┤
│ ← │ t │ → │ = │ ▸ │
└───┴───┴───┴───┴───┘
*/

const Action fn1Layer[20] = {
  ACT_ADD, ACT_SUB, ACT_MUL, ACT_MEM, ACT_DEL,
  ACT_DIV, ACT_MOD, ACT_NOT, ACT_FN1, ACT_FN2,
  ACT_AND, ACT_OR,  ACT_XOR, ACT_LT,  ACT_GT,
  ACT_LT,  ACT_GT,  ACT_EQ,  ACT_EQ,  ACT_ENTER
};

/*
┌───┬───┬───┬───┬───┐
│ a │ b │ c │MEM│DEL│
├───┼───┼───┼───┼───┤
│ d │ e │ f │FN1│FN2│
├───┼───┼───┼───┼───┤
│ ? │ : │ " │ ( │ ) │
├───┼───┼───┼───┼───┤
│ ← │   │ → │ ; │ ▸ │
└───┴───┴───┴───┴───┘
*/
const Action fn2Layer[20] = {
  ACT_ALPHA_A,  ACT_ALPHA_B, ACT_ALPHA_C, ACT_MEM,        ACT_DEL,
  ACT_ALPHA_D,  ACT_ALPHA_E, ACT_ALPHA_F, ACT_FN1,        ACT_FN2,
  ACT_QUESTION, ACT_COLON,   ACT_QUOTE,   ACT_LEFT_PAREN, ACT_RIGHT_PAREN,
  ACT_LEFT,     ACT_NONE,    ACT_RIGHT,   ACT_SEMICOLON,  ACT_ENTER
};

/*
┌───┬───┬───┬───┬───┐
│P7 │P8 │P9 │MEM│DEL│
├───┼───┼───┼───┼───┤
│P4 │P5 │P6 |FN1│FN2│
├───┼───┼───┼───┼───┤
│P3 │P2 │P1 │   │   │
├───┼───┼───┼───┼───┤
│P- │   │P+ │   │SAV│
└───┴───┴───┴───┴───┘
*/
const Action memLayer[20] = {
  ACT_PRESET_7,   ACT_PRESET_8, ACT_PRESET_9, ACT_MEM,    ACT_DEL,
  ACT_PRESET_4,   ACT_PRESET_5, ACT_PRESET_6, ACT_FN1,    ACT_FN2,
  ACT_PRESET_3,   ACT_PRESET_2, ACT_PRESET_1, ACT_NONE,   ACT_NONE,
  ACT_PRESET_DEC, ACT_NONE,     ACT_PRESET_INC, ACT_NONE, ACT_SAVE
};

KeyMode currentMode = MODE_BASE;

Token expr[MAX_TOKENS];
volatile CompileError compileError = ERR_NONE;
uint8_t expr_len = 0;
char textBuffer[TEXT_BUFFER_SIZE];
uint8_t text_len = 0;
uint8_t cursor = 0;
bool oledDirty = false;
bool needsRecompile = false;
bool needsResetT = false;
bool toasterVisible = false;

// Toaster message system
char toasterMsg[32] = {0};
unsigned long toasterStartTime = 0;
#define TOASTER_DURATION 2000 // ms

void showToaster(const char* msg) {
  strncpy(toasterMsg, msg, sizeof(toasterMsg) - 1);
  toasterMsg[sizeof(toasterMsg) - 1] = '\0';
  toasterStartTime = millis();
  toasterVisible = true;
  oledDirty = true;
}

// Splash screen control
bool splashScreen = true;

// Play/Stop state
bool isPlaying = false;

volatile uint32_t t = 0;
unsigned long lastKeyTime = 0;

uint8_t current_slot = 0;

// ===================== PRESETS =====================
const char preset0[] PROGMEM = "t*(42&t>>10)";
const char preset1[] PROGMEM = "t*((t>>12)|(t>>8))";
const char preset2[] PROGMEM = "t*(0xdeadbeef>>(t>>11)&15)/2|t>>3|t>>(t>>10)";
const char preset3[] PROGMEM = "(t>>8|t>>9|t>>10)*((t>>5)&(t>>6))";
const char preset4[] PROGMEM = "(t>>7|t>>5|t>>4)*((t>>11)&(t>>9))";
const char preset5[] PROGMEM = "(t>>6|t>>4|t>>2)*((t>>9)&(t>>8))";
const char preset6[] PROGMEM = "(t>>5|t>>3|t>>1)*((t>>7)&(t>>6))";
const char preset7[] PROGMEM = "(t>>4|t>>2|t>>0)*((t>>5)&(t>>4))";
const char preset8[] PROGMEM = "(t>>3|t>>1|t>>0)*((t>>3)&(t>>2))";

const char* const factoryPresets[] PROGMEM = {
  preset0,
  preset1,
  preset2,
  preset3,
  preset4,
  preset5,
  preset6,
  preset7,
  preset8
};

void clearAllPresets() {
  for (uint8_t slot = 0; slot < PRESET_COUNT; slot++) {
    // Write 0xFF to first byte to mark as empty
    EEPROM.update(SLOT_ADDR(slot), 0xFF);
  }
}

bool isSlotEmpty(uint8_t slot) {
  return EEPROM.read(SLOT_ADDR(slot)) == 0xFF;
}

bool loadPreset(uint8_t slot, char* exprBuffer) {
  if (slot >= PRESET_COUNT) return false;
  
  char msg[16];
  sprintf(msg, "Loaded P%d", slot);
  showToaster(msg);

  if (isSlotEmpty(slot)) {
    // Load factory preset if available
    if (slot < (sizeof(factoryPresets) / sizeof(factoryPresets[0]))) {
      strcpy_P(exprBuffer,
               (PGM_P)pgm_read_ptr(&factoryPresets[slot]));
    } else {
      exprBuffer[0] = 0; // empty expression
    }

    text_len = strlen(exprBuffer);
    cursor = text_len;
    current_slot = slot;
    needsResetT = true;
    
    return true;
  }

  // Load from EEPROM
  for (int i = 0; i < TEXT_BUFFER_SIZE; i++) {
    char c = EEPROM.read(SLOT_ADDR(slot) + i);
    exprBuffer[i] = c;
    if (c == 0) {
      text_len = i;
      cursor = i;
      current_slot = slot; // Update current slot when loading from EEPROM
      break;
    }
  }

  return true;
}

bool savePreset(uint8_t slot, const char* exprBuffer) {
  if (slot >= PRESET_COUNT) return false;
  
  char msg[16];
  sprintf(msg, "Saved P%d", slot);
  showToaster(msg);

  for (int i = 0; i < TEXT_BUFFER_SIZE; i++) {
    char c = exprBuffer[i];
    EEPROM.update(SLOT_ADDR(slot) + i, c);
    if (c == 0) break;
  }

  current_slot = slot; // Update current slot after saving
  return true;
}

// ===================== MATRIX =====================
const byte ROWS = 4;
const byte COLS = 5;

const byte rows[ROWS] = {2, 3, 4, 5};
const byte cols[COLS] = {6, 7, 8, 9, 10};

// Key state tracking
uint8_t keyStates[20] = {0}; // 0=up, 1=pressed
uint8_t keyStatesPrev[20] = {0};

void scanMatrix() {
  // Save previous states
  memcpy(keyStatesPrev, keyStates, sizeof(keyStates));
  
  // Scan all keys
  for (int r = 0; r < ROWS; r++) {
    digitalWrite(rows[r], LOW);
    delayMicroseconds(5); // settle

    for (int c = 0; c < COLS; c++) {
      uint8_t keyIndex = r * COLS + c;
      keyStates[keyIndex] = (digitalRead(cols[c]) == LOW) ? 1 : 0;
    }
    digitalWrite(rows[r], HIGH);
  }
}

uint8_t getPressedKey() {
  // Return first newly pressed key (edge detection)
  for (uint8_t i = 0; i < 20; i++) {
    if (keyStates[i] == 1 && keyStatesPrev[i] == 0) {
      return i; // Key just pressed
    }
  }
  return 255; // No new key press
}

bool isKeyPressed(uint8_t key) {
  return (key < 20) ? (keyStates[key] == 1) : false;
}

void setupMatrix() {
  for (int r = 0; r < ROWS; r++) {
    pinMode(rows[r], OUTPUT);
    digitalWrite(rows[r], HIGH); // idle
  }
  for (int c = 0; c < COLS; c++) {
    pinMode(cols[c], INPUT_PULLUP);
  }
}

// ===================== EDITOR =====================
bool insertChar(char c) {
  if (text_len >= TEXT_BUFFER_SIZE - 1) return;

  for (int i = text_len; i > cursor; i--) {
    textBuffer[i] = textBuffer[i - 1];
  }

  textBuffer[cursor] = c;
  text_len++;
  cursor++;
  textBuffer[text_len] = '\0';
  return true;
}

bool insertString(char* text) {
  uint8_t len = strlen(text);
  if (text_len + len >= TEXT_BUFFER_SIZE) return;
  for (uint8_t i = 0; i < len; i++) {
    insertChar(text[i]);
  }
  return true;
}

bool deleteChar() {
  if (cursor == 0 || text_len == 0) return;

  for (int i = cursor - 1; i < text_len - 1; i++) {
    textBuffer[i] = textBuffer[i + 1];
  }

  text_len--;
  cursor--;
  textBuffer[text_len] = '\0';
  
  return true;
}

bool insertDigit(uint8_t digit) {
  insertChar('0' + digit);
  return true;
}

bool insertOperator(char op) {
  insertChar(op);
  return true;
}

bool insertT() {
  insertChar('t');
  return true;
}

Action resolveAction(uint8_t key) {
  if (key >= 20) return ACT_NONE;

  if (currentMode == MODE_FN1) {
    return fn1Layer[key];
  }

  if (currentMode == MODE_FN2) {
    return fn2Layer[key];
  }

  if (currentMode == MODE_MEM) {
    return memLayer[key];
  }

  return baseLayer[key];
}

bool executeAction(Action a) {
  switch (a) {
    case ACT_DIGIT_0: return insertDigit(0);
    case ACT_DIGIT_1: return insertDigit(1);
    case ACT_DIGIT_2: return insertDigit(2);
    case ACT_DIGIT_3: return insertDigit(3);
    case ACT_DIGIT_4: return insertDigit(4);
    case ACT_DIGIT_5: return insertDigit(5);
    case ACT_DIGIT_6: return insertDigit(6);
    case ACT_DIGIT_7: return insertDigit(7);
    case ACT_DIGIT_8: return insertDigit(8);
    case ACT_DIGIT_9: return insertDigit(9);

    case ACT_ADD: return insertOperator('+');
    case ACT_MUL: return insertOperator('*');
    case ACT_SUB: return insertOperator('-');
    case ACT_DIV: return insertOperator('/');
    case ACT_MOD: return insertOperator('%');

    case ACT_AND: return insertOperator('&');
    case ACT_OR:  return insertOperator('|');
    case ACT_XOR: return insertOperator('^');

    case ACT_LT: return insertOperator('<');
    case ACT_GT: return insertOperator('>');
    case ACT_EQ: return insertOperator('=');
    case ACT_DOT: return insertOperator('.');
    case ACT_QUOTE: return insertOperator('\"');
    case ACT_COLON: return insertOperator(':');
    case ACT_SEMICOLON: return insertOperator(';');
    case ACT_QUESTION: return insertOperator('?');
    case ACT_0B: return insertString("0b");
    case ACT_0X: return insertString("0x");
    case ACT_NOT: return insertOperator('~');

    case ACT_DEL: return deleteChar();
    case ACT_LEFT: if (cursor > 0) cursor--; break;
    case ACT_RIGHT: if (cursor < text_len) cursor++; break;
    case ACT_T: return insertT();
    case ACT_LEFT_PAREN: return insertOperator('(');
    case ACT_RIGHT_PAREN: return insertOperator(')');

    case ACT_PRESET_1: return loadPreset(0, textBuffer);
    case ACT_PRESET_2: return loadPreset(1, textBuffer);
    case ACT_PRESET_3: return loadPreset(2, textBuffer);
    case ACT_PRESET_DEC: 
      if (current_slot > 0) {
        current_slot--;
        return loadPreset(current_slot, textBuffer);
      }
      break;
    case ACT_PRESET_INC:
      if (current_slot < PRESET_COUNT - 1) {
        current_slot++;
        return loadPreset(current_slot, textBuffer);
      }
      break;

    case ACT_SAVE: return savePreset(current_slot, textBuffer);

    case ACT_ENTER: 
      isPlaying = !isPlaying;
      if (isPlaying) {
        t = 0; // Reset time when starting playback
        // Enable audio output
        TCCR2A |= _BV(COM2A1);
      } else {
        // Disable audio output
        TCCR2A &= ~_BV(COM2A1);
        OCR2A = 128; // Center value (silence)
      }
      return false;

    case ACT_ALPHA_A: return insertChar('a');
    case ACT_ALPHA_B: return insertChar('b');
    case ACT_ALPHA_C: return insertChar('c');
    case ACT_ALPHA_D: return insertChar('d');
    case ACT_ALPHA_E: return insertChar('e');
    case ACT_ALPHA_F: return insertChar('f');

    case ACT_FN1: {
      currentMode = (currentMode == MODE_FN1) ? MODE_BASE : MODE_FN1;
      return true;
    }
    case ACT_FN2: {
      currentMode = (currentMode == MODE_FN2) ? MODE_BASE : MODE_FN2;
      return true;
    }
    case ACT_MEM: {
      currentMode = (currentMode == MODE_MEM) ? MODE_BASE : MODE_MEM;
      return true;
    }

    default: break;
  }

  return false;
}

// ===================== AUDIO TIMER =====================

void setupAudio() {
  pinMode(11, OUTPUT); // OC2A

  // Timer2: Fast PWM, no prescaler (~31.25 kHz carrier)
  TCCR2A = _BV(COM2A1) | _BV(WGM20) | _BV(WGM21);
  TCCR2B = _BV(CS20); // prescaler = 1

  OCR2A = 0; // duty (audio)
  
  // Enable overflow interrupt
  TIMSK2 = _BV(TOIE2);
}


// ===================== RPN VM =====================
#define RPN_STACK_SIZE 8
#define RPN_PROGRAM_SIZE 32

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
  uint32_t value; // for PUSH_NUM
};

RpnInstruction rpnProgram[RPN_PROGRAM_SIZE];
uint8_t rpnProgramLen = 0;

// Helper function to check if character is a hex digit
bool isHexDigit(char c) {
  return (c >= '0' && c <= '9') || 
         (c >= 'a' && c <= 'f') || 
         (c >= 'A' && c <= 'F');
}

// Compile text to RPN using shunting-yard algorithm
uint8_t compileToRPN() {
  compileError = ERR_NONE;
  rpnProgramLen = 0;
  
  // Operator stack for shunting-yard
  uint8_t opStack[16];
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
      
      rpnProgram[rpnProgramLen++] = { RPN_PUSH_NUM, num };
      expectOperand = false;
      continue;
    }
    
    // Variable t
    if (c == 't') {
      if (!expectOperand) {
        compileError = ERR_TOKEN;
        return 0;
      }

      rpnProgram[rpnProgramLen++] = { RPN_PUSH_T, 0 };
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
        opcode = 255;
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
        opcode = 254;
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
      case '+': case '-': precedence = getPrecedence(RPN_ADD); opcode = (c == '+') ? RPN_ADD : RPN_SUB; isBinaryOp = true; break;
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
    
    if (opcode == 255) { // '('
      opStack[opStackTop++] = opcode;
      i++;
    } else if (opcode == 254) { // ')'
      // Pop until '('
      while (opStackTop > 0 && opStack[opStackTop-1] != 255) {
        rpnProgram[rpnProgramLen].opcode = opStack[--opStackTop];
        rpnProgram[rpnProgramLen].value = 0;
        rpnProgramLen++;
        }
      if (opStackTop > 0) opStackTop--; // remove '('
      i++;
      } else {
      // Pop higher precedence operators
      while (opStackTop > 0 && opStack[opStackTop-1] != 255 && 
             (!rightAssoc && precedence <= getPrecedence(opStack[opStackTop-1]) ||
              rightAssoc && precedence < getPrecedence(opStack[opStackTop-1]))) {
        if (rpnProgramLen < RPN_PROGRAM_SIZE) {
          rpnProgram[rpnProgramLen].opcode = opStack[--opStackTop];
          rpnProgram[rpnProgramLen].value = 0;
          rpnProgramLen++;
        }
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
    rpnProgram[rpnProgramLen].opcode = opStack[--opStackTop];
    rpnProgram[rpnProgramLen].value = 0;
    rpnProgramLen++;
  }
  
  return rpnProgramLen;
}

uint8_t getPrecedence(uint8_t opcode) {
  switch (opcode) {
    case RPN_NOT: return 7;          // ~

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


// Execute RPN program
uint32_t executeRPN(uint32_t tval) {
  uint32_t stack[RPN_STACK_SIZE];
  uint8_t stackTop = 0;
  
  for (uint8_t pc = 0; pc < rpnProgramLen; pc++) {
    uint8_t opcode = rpnProgram[pc].opcode;
    
    switch (opcode) {
      case RPN_PUSH_T:
        if (stackTop < RPN_STACK_SIZE) stack[stackTop++] = tval;
        break;
        
      case RPN_PUSH_NUM:
        if (stackTop < RPN_STACK_SIZE) stack[stackTop++] = rpnProgram[pc].value;
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
        
      case RPN_SHL:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop]
          uint32_t a = stack[--stackTop];
          stack[stackTop++] = a << b;
        }
        break;

      case RPN_SHR:
        if (stackTop >= 2) {
          uint32_t b = stack[--stackTop]
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

// ===================== LOAD INDICATOR =====================
// Removed - too flaky
// ===================== AUDIO ISR =====================
ISR(TIMER2_OVF_vect) {
  static uint8_t div = 0;

  // 31.25 kHz / 4 ≈ 7.8 kHz
  if (++div == 4) {
    if (isPlaying) {
      OCR2A = (executeRPN(t) >> 1) + 128;
      t++;
    } else {
      OCR2A = 128; // Silence when not playing
    }
    div = 0;
  }
}

// ===================== SETUP =====================

void setup() {
  // Initialize U8g2 display (PAGE BUFFER MODE)
  display.begin();
  display.setBusClock(400000); // 400kHz I2C (good)
  setupMatrix();
  
  // Check if FN1 and FN2 are both pressed on boot
  scanMatrix();
  if (isKeyPressed(KEY_FN2)) {
    clearAllPresets();
    
    // Show clear confirmation
    display.firstPage();
    do {
      display.setFont(u8g2_font_6x10_tf);
      display.setCursor(0, 12);
      display.print(F("All presets cleared!"));
      display.setCursor(0, 24);
      display.print(F("Release to continue"));
    } while (display.nextPage());
    
    // Wait for key to be released
    while (isKeyPressed(KEY_FN2)) {
      scanMatrix();
      delay(50);
    }
  }
  
  setupAudio();
  
  // Initialize with example expression
  loadPreset(0, textBuffer);
  needsRecompile = true;
  
  display.firstPage();
  do {
    display.setFont(u8g2_font_6x10_tf);
    display.setCursor(0, 12);
    display.print(F("BytebeatPocket ready!"));
  } while (display.nextPage());
}

const char* getModeLabel() {
  switch (currentMode) {
    case MODE_FN1:  return "FN1";
    case MODE_FN2:  return "FN2";
    case MODE_MEM:  return "MEM";
    default:        return "BASE";
  }
}

void drawErrorBanner() {
  display.setDrawColor(1);
  display.drawBox(0, 52, 128, 12);
  display.setDrawColor(0);
  display.setCursor(2, 62);

  switch (compileError) {
    case ERR_PAREN: display.print("ERR: PAREN"); break;
    case ERR_STACK: display.print("ERR: STACK"); break;
    case ERR_TOKEN: display.print("ERR: TOKEN"); break;
    case ERR_PROGRAM_TOO_LONG: display.print("ERR: TOO LONG"); break;
    default: break;
  }

  display.setDrawColor(1);
}

void drawExprOLED() {
  // Show splash screen until first interaction
  if (splashScreen) {
    return;
  }

  display.firstPage();
  do {
    display.setFont(u8g2_font_6x10_tf);
    
    // Draw current slot on left
    display.setDrawColor(1);
    display.setCursor(0, 8);
      char slotStr[8];
      sprintf(slotStr, "P%d", current_slot);
      display.print(slotStr);
    
    // Draw play/stop indicator in center
    display.setCursor(50, 8);
    if (isPlaying) {
      display.print(F("PLAY"));
    } else {
      display.print(F("STOP"));
    }
    
    // Draw mode indicator on right
    const char* modeLabel = getModeLabel();
    uint8_t modeWidth = display.getStrWidth(modeLabel);
    uint8_t modeX = SCREEN_WIDTH - modeWidth - 2;
    
    if (currentMode == MODE_BASE) {
      display.setCursor(modeX, 8);
      display.print(modeLabel);
    } else {
      display.drawBox(modeX - 2, 0, modeWidth + 4, 10);
      display.setDrawColor(0);
      display.setCursor(modeX, 8);
      display.print(modeLabel);
      display.setDrawColor(1);
    }
    
    // Draw toaster message if active
    if (toasterVisible) {
      display.setDrawColor(1);
      display.drawBox(20, 53, SCREEN_WIDTH - 40, 12);
      display.setDrawColor(0);
      display.setCursor((SCREEN_WIDTH - display.getStrWidth(toasterMsg)) / 2, 62);
      display.print(toasterMsg);
      display.setDrawColor(1);
      toasterVisible = true;
    } else if (compileError != ERR_NONE) {
      drawErrorBanner();
    }
    
    // Text buffer with line wrap and simple cursor
    uint8_t line = 0;
    uint8_t col = 0;
    uint8_t charIndex = 0;
    
    while (charIndex < text_len && line < 5) {
      char c = textBuffer[charIndex];
      
      // Check if we need to wrap to next line
      if (col >= 21) { // Approx 21 chars per line at 6px font
        line++;
        col = 0;
        if (line >= 5) break; // No more space
      }
      
      uint8_t x = col * 6;
      uint8_t y = 20 + line * 10; // Start after first line

      bool isCursor = (charIndex == cursor);
      
      if (isCursor) {
        // Inverted cursor
        display.setDrawColor(1);
        display.drawBox(x, y - 8, 6, 10);
        display.setDrawColor(0);
        display.setCursor(x, y);
        display.print(c);
        display.setDrawColor(1);
      } else {
        // Normal text
        display.setCursor(x, y);
        display.print(c);
      }
      
      col++;
      charIndex++;
    }
  } while (display.nextPage());
}

// ===================== LOOP =====================
void loop() {
  static uint8_t lastKey = 255;
  unsigned long now = millis();

  // Scan matrix and detect all key states
  scanMatrix();
  
  // Get newly pressed key (edge detection)
  uint8_t k = getPressedKey();
  bool pressed = (k != 255);

  if (pressed && k != lastKey && (now - lastKeyTime) > 120) {
    if (splashScreen) {
      splashScreen = false;
      return;
    }

    Action a = resolveAction(k);
    if (a != ACT_NONE) {
      needsRecompile = executeAction(a);
      oledDirty = true;
    }

    lastKeyTime = now;
  }

  if (!pressed && needsRecompile) {
    cli();
    uint8_t len = compileToRPN();
    if (needsResetT) {
      t = 0;
      needsResetT = false;
    }
    sei();
    rpnProgramLen = (compileError == ERR_NONE) ? len : 0;
    needsRecompile = false;
  }
  
  // Update display if toaster expires
  if (toasterVisible && !oledDirty) {
    if ((millis() - toasterStartTime) > TOASTER_DURATION) {
      oledDirty = true;
      toasterVisible = false;
    }
  }

  if (oledDirty && !needsRecompile) {
    drawExprOLED();
    oledDirty = false;
  }

  lastKey = k;
}

