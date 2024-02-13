# Port of Lox language to Arduino GIGA R1 WiFi (and other boards)

## Description

An experimental project to determine whether a full-featured scripting language can be used on larger-memory Arduino boards to control hardware. At the moment only the Giga board with Arduino Display Shield is tested, but support for Portenta H7 (with USB-C video display) may be easy to add.

The [Lox language](https://www.craftinginterpreters.com/appendix-i.html) was chosen because of the availability of a ready-made implementation in C ([clox](https://github.com/munificent/craftinginterpreters/tree/master/c)) which is a compact and very quick JIT-compiled interpreter. It is also easy to extend with additional Lox functions, which are mapped to native C/C++ ones defined in the sketch. The construction of the interpreter is described in detail in the book "Crafting Interpreters", however reading the book is not necessarily a prerequisite for using the language or even extending it with new functions.

## Getting Started

The functionality of the sketch has been designed to be switchable, see the file `clox_gfx_config.h` in the "examples/clox_gfx_demo" directory. 

* To enable `CLOX_GRAPHICS` a suitable board with support for Arduino_H7_Video is required: Portenta H7 with USBCVideo (untested) or GIGA R1 WiFi with GigaDisplayShield.
* To enable `CLOX_USB_HOST` a GIGA R1 WiFi is required, this allows reading sketches from a memory stick plugged into the host USB port on the board.
* To enable `CLOX_WEB_CONSOLE` support for the `WebSockets2_Generic` library is required, for GIGA R1 WiFi board this means a [patched version 1.14+](https://github.com/cpp-tutor/WebSockets2_Generic)
* To enable `CLOX_USE_SDRAM` a suitable board with support for Portenta_SDRAM is required.

The number of supported graphics functions (and other functions from [this page](https://www.arduino.cc/reference/en/)) is already quite large, leading to a 600+ line sketch required to support the library functionality. Take a look at the example "clox_gfx_demo" and copy it into your sketches folder; the functions prefixed with `gfx_` such as `gfx_millis` are called from within the Lox interpreter without this prefix, for example as `print millis();`.

Flashing and booting the Giga results in a REPL in the Serial Monitor, and also in the Web Console accessed from the address printed when booting, enter line(s) of Lox code at `> ` (start) and `. ` (continuation) prompts, and press Enter on a blank line to execute. Error messages are reported in the REPL, and the blue LED turns on for the duration of executing the code fragment just entered. To pulse the blue LED for ten seconds use the following Lox code in the interpreter:

```javascript
var led = 88;
pinMode(led, "OUTPUT");
for (var i = 1; i <= 10; i = i + 1) {
  delay(500);
  digitalWrite(led, false);
  delay(500);
  digitalWrite(led, true);
}
```

Graphics on the Giga Display are also supported via the ArduinoGraphics library (note: **not** Arduino_GigaDisplay_GFX). To draw graphics, wrap the commands inside `beginDraw();` and `endDraw();`, for example:

```javascript
fun logo(center_x, center_y) {
  beginDraw();
  background(255, 255, 255);
  clear();
  fill(0, 129, 132);
  circle(center_x, center_y, 300);
  stroke(255, 255, 255);
  noFill();
  for (var i = 0; i < 30; i = i + 1) {
    circle(center_x-55+5, center_y, 110-i);
    circle(center_x+55-5, center_y, 110-i);
  }
  fill(255, 255, 255);
  rect(center_x-55-16+5, center_y-5, 32, 10);
  fill(255, 255, 255);
  rect(center_x+55-16-5, center_y-5, 32, 10);
  fill(255, 255, 255);
  rect(center_x+55-5-5, center_y-16, 10, 32);
  endDraw();
}

fun animate(seconds) {
  var x_speed = 4;
  var y_speed = 4;
  var time_limit = millis() + seconds * 1000;
  var x = width() / 2;
  var y = height() / 2;
  while (millis() < time_limit) {
    logo(x, y);
    delay(10);
    x = x + x_speed;
    y = y + y_speed;
    if ((x + 150) > width()) {
      x = width() - 150;
      x_speed = -x_speed;
    }
    if ((y + 150) > height()) {
      y = height() - 150;
      y_speed = -y_speed;
    }
    if (x < 150) {
      x = 150;
      x_speed = -x_speed;
    }
    if (y < 150) {
      y = 150;
      y_speed = -y_speed;
    }
  }
}

animate(20);
```

Note: The Arduino_H7_Video library uses some shared SDRAM for the framebuffer, and `SDRAM.begin();` should **not** be called after initializing the display.

Scripts stored on USB flash devices plugged into the USB port on the Giga (or via an OTG cable on Portenta H7) can be loaded with `load "script.lox"` at the prompt (any file extension can be used).

## Adding Functions

The process of adding additional native functions to the Lox interpreter has four stages:

1. Write the C++ function with `gfx_` prefix in the script. The return value should be one of `GFX_RETURN_NIL`, `GFX_RETURN_NUM(double)` or `GFX_RETURN_BOOL(bool)`, the parameters need to be convertible from double (unless using custom `GFX_ARGS_...`). This function **must** be declared with C-linkage (inside an `extern "C"` block).

2. Copy the prototype for this function into `clox_gfx.h` and make sure one of the `#define GFX_DEFINE_...` choices matches this prototype.

3. Add `GFX_DEFINE(name, arity)` or `GFX_DEFINE_...(name)` to `vm.c` after line ~105 (the existing native function definitions)

4. Add `GFX_DECLARE(name);` to `vm.c` after line ~175 (in definition of `initVM()`)

Doing these steps correctly and in order ensures that the project should remain compilable at all times. Look out for both compilation and linker errors.

## Future Developments

There are a number of ideas for the future direction of this library:

* Stability: likely to be many bugs in non-core library code, and faulty Lox input can crash the board (fixed)
* Complete support for more "Arduino.h" functions
* Support for Arduino_GigaDisplayTouch
* Support for running scripts via a web interface (console-in-a-web-page) (#define CLOX_WEB_CONSOLE 1)
* Use of the M4 co-processor as a graphics accelerator
* Changing the Lox interpreter language (not the JIT backend) to something less like JavaScript (GFX-Basic?)
* Adding to the ArduinoGraphics library (filled triangles, more fonts etc.)

If you have any other suggestions or ideas, please raise feature requests as issues.

## DIY

In the style of the book "Crafting Interpreters" by Bob Nystrom, which describes the creation of clox (and jlox) from scratch, here is a checklist of how to reproduce the work so far:

1. Clone (part of) the GitHub repo munificent/craftinginterpreters: `svn export https://github.com/munificent/craftinginterpreters.git/trunk/c`

2. Make a folder in your "libraries" directory and copy all of these checked-out files into it, apart from `main.c`.

3. Change all occurences of `#include <stdio.h>` to `#include "clox_stdio.h"` in files: `compiler.c`, `debug.c`, `memory.c`, `object.c`, `scanner.c`, `value.c` and `vm.c`

4. Add this code as a new file `clox_stdio.h` to the same folder:

```c
#ifndef clox_stdio_h
#define clox_stdio_h

#include <stdarg.h>
#include <stdio.h>

#define printf Serial_printf
#define fprintf Serial_fprintf
#define vfprintf Serial_vfprintf
#define fputs(output, stream) Serial_fprintf(stream, "%s", output)

int Serial_printf(const char *fmt, ...);
int Serial_fprintf(FILE *dummy, const char *fmt, ...);
int Serial_vfprintf(FILE *dummy, const char *fmt, va_list args);

#endif
```

5. Create a script which defines these three functions, and includes the necessary C headers (as a minimum). Here is a possible outline:

```ino
extern "C" {
#include "vm.h"
#include "clox_stdio.h"
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    delay(100);
  }

  pinMode(LEDB, OUTPUT);
  digitalWrite(LEDB, LOW);
  Serial_printf("Initializing clox VM...\n");
  delay(1000);
  initVM();
  digitalWrite(LEDB, HIGH);
}

void loop() {
  Serial_printf("> ");
  String input, line;
  while (line != "\n") {
    line = "";
    char ch = '\0';
    while (ch != '\n') {
      if (Serial.available()) {
        ch = Serial.read();
        line += ch;
      }
    }
    input += line;
    Serial_printf("%s", line.c_str());
    if (line != "\n") {
      Serial_printf(". ");
    }
  }

  digitalWrite(LEDB, LOW);
  interpret(input.c_str());
  digitalWrite(LEDB, HIGH);
}

extern "C" {

int Serial_printf(const char *fmt, ...) {
  if (Serial) {
    va_list args;
    va_start(args, fmt);
    int nchars = Serial_vfprintf(stdout, fmt, args);
    va_end(args);
    return nchars;
  }
  else {
    return 0;
  }
}

int Serial_fprintf(FILE *dummy, const char *fmt, ...) {
  if (Serial) {
    va_list args;
    va_start(args, fmt);
    int nchars = Serial_vfprintf(dummy, fmt, args);
    va_end(args);
    return nchars;
  }
  else {
    return 0;
  }
}

int Serial_vfprintf(FILE *dummy, const char *fmt, va_list args) {
  if (Serial) {
    va_list(args2);
    va_copy(args2, args);
    int nchars = vsnprintf(nullptr, 0, fmt, args2);
    va_end(args2);
    char *buf = (char *)malloc(nchars + 1);
    vsprintf(buf, fmt, args);
    Serial.print(buf);
    free(buf);
    Serial.flush();
    return nchars;
  }
  else {
    return 0;
  }
}

} // extern "C"
```

Check that this compiles and link correctly, and then boots to a basic REPL in the Serial Monitor. You can now begin adding native functions to be called by Lox, as outlined above.

## License

This software is released under the MIT License, as is all the code from the "Crafting Interpreters" book.
