# BytebeatPocket

A handheld bytebeat synthesizer based on Raspberry Pico 2.

## Features

### Operators

- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Bitwise: `&`, `|`, `^`, `~`
- Shifts: `<<`, `>>`
- Comparisons: `<`, `>`, `<=`, `>=`, `==`, `!=`
- Parentheses: `(`, `)`

### Numbers

- Decimal: `123`, `42`
- Hexadecimal: `0xDEAD`, `0xBEEF`
- Binary: `0b101010`, `0b11001100`

### Variables

- `t`: Time variable that increments each sample

## Hardware Wiring

### OLED Display (SSD1306 128x64 I2C)

| OLED Pin | Pico Pin | GPIO |
|----------|----------|------|
| VCC      | 3V3(OUT) | Pin 36 or VSYS Pin 39 (5V) |
| GND      | GND      | Pin 38 (or any GND) |
| SDA      | GPIO 4   | Pin 6 |
| SCL      | GPIO 5   | Pin 7 |

**Note**: The OLED can work with either 3.3V or 5V power depending on your module. Try 5V (VSYS) if 3.3V doesn't work reliably.

### Button Matrix (4x5 = 20 keys)

**Rows (Outputs - driven LOW to scan):**
| Row | Pico Pin | GPIO |
|-----|----------|------|
| 0   | GPIO 16  | Pin 21 |
| 1   | GPIO 17  | Pin 22 |
| 2   | GPIO 18  | Pin 24 |
| 3   | GPIO 19  | Pin 25 |

**Columns (Inputs - with pullups):**
| Col | Pico Pin | GPIO |
|-----|----------|------|
| 0   | GPIO 20  | Pin 26 |
| 1   | GPIO 21  | Pin 27 |
| 2   | GPIO 22  | Pin 29 |
| 3   | GPIO 26  | Pin 31 |
| 4   | GPIO 27  | Pin 32 |

### Audio Output

| Function | Pico Pin | GPIO |
|----------|----------|------|
| Audio PWM | GPIO 0  | Pin 1 |
| GND       | GND     | Pin 3 (or any GND) |

Connect GPIO 0 through a low-pass filter (e.g., 1kΩ resistor + 10µF capacitor) to your speaker/amplifier.

### Button Matrix Layout

```
┌───┬───┬───┬───┬───┐
│ 7 │ 8 │ 9 │MEM│DEL│  Row 0
├───┼───┼───┼───┼───┤
│ 4 │ 5 │ 6 │FN1│FN2│  Row 1
├───┼───┼───┼───┼───┤
│ 1 │ 2 │ 3 │0b │0x │  Row 2
├───┼───┼───┼───┼───┤
│ ← │ 0 │ → │ . │ ▸ │  Row 3
└───┴───┴───┴───┴───┘
 C0  C1  C2  C3  C4
```

**Layer Switching:**

- **FN1**: Hold to access operators (+, -, *, /, %, &, |, ^, ~, <, >, =, t)
- **FN2**: Hold to access hex digits (a-f) and symbols (?, :, ", (, ), ;)
- **MEM**: Hold to access preset slots (P1-P9, P+, P-, SAVE)

## Building

```bash
cd build
cmake ..
cmake --build .
```

Flash the resulting `bytebeat_pocket.uf2` file to your Pico by holding BOOTSEL while plugging in USB.

## Usage

Connect to the Pico via USB serial (115200 baud) and use these commands:

```
> help                    # Show available commands
> play                    # Start audio playback
> stop                    # Stop audio playback
> expr t*(42&t>>10)       # Set bytebeat expression
> expr t*((t>>12)|(t>>8)) # Another example
> expr t*(0xdeadbeef>>(t>>11)&15)/2|t>>3|t>>(t>>10)
```
