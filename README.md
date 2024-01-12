# Port of Lox language to Arduino GIGA R1 Wifi

## Description

An experimental project to determine whether a full-featured scripting language can be used on larger-memory Arduino boards to control hardware. At the moment only the Giga board with Arduino Display Shield is supported, but future support for Portenta H7 (with USB-C display) may be possible.

The [Lox language](https://www.craftinginterpreters.com/appendix-i.html) was chosen because of the availability of a ready-made implementation in C ([clox](https://github.com/munificent/craftinginterpreters/tree/master/c)) which is a compact and very quick JIT-compiled interpreter. It is also easy to extend with additional Lox functions, which are mapped to native C/C++ ones defined in the sketch. The construction of the interpreter is described in detail in the book "Crafting Interpreters", however reading the book is not necessarily a prerequisite for using the language or even extending it with new functions.

## Getting Started

The number of supported graphics functions (and other functions from [this page](https://www.arduino.cc/reference/en/)) is already quite large, leading to a near 400-line sketch required to support the library functionality. Take a look at the example "clox_gfx_demo" and copy it into your sketches folder; the functions prefixed with `gfx_` such as `gfx_millis` are called from within the Lox interpreter without this prefix, for example as `print millis();`.

Flashing and booting the Giga results in a REPL in the Serial Monitor, enter line(s) of Lox code at `> ` (start) and `. ` (continuation) prompts, and press Enter on a blank line to execute. Error messages are reported in the REPL, and the blue LED turns on for the duration of executing the code fragment just entered. To pulse the blue LED for ten seconds use the following Lox code in the interpreter:

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
beginDraw();
fill(127, 127, 255);
ellipse(400, 240, 200, 100);
endDraw();
```

Note: The Arduino_H7_Video library uses some shared SDRAM for the framebuffer, and `SDRAM.begin();` should **not** be called after initializing the display.

Scripts stored on USB flash devices plugged into the USB port on the Giga can be loaded with `load "script.lox"` at the prompt.

## Adding Functions

The process of adding additional native functions to the Lox interpreter has four stages:

1. Write the C++ function with `gfx_` prefix in the script. The return value should be one of `GFX_RETURN_NIL`, `GFX_RETURN_NUM(double)` or `GFX_RETURN_BOOL(bool)`, the parameters need to be convertible from double (unless using custom `GFX_ARGS_...`). This function **must** be declared with C-linkage (inside an `extern "C"` block).

2. Copy the prototype for this function into `clox_gfx.h` and make sure one of the `#define GFX_DEFINE_...` choices matches this prototype.

3. Add `GFX_DEFINE(name, arity)` or `GFX_DEFINE_...(name)` to `vm.c` after line ~105 (the existing native function definitions)

4. Add `GFX_DECLARE(name);` to `vm.c` after line ~175 (in definition of `initVM()`)

Doing these steps correctly and in order ensures that the project should remain compilable at all times. Look out for both compilation and linker errors.

## Future Developments

There are a number of ideas for the future direction of this library:

* Stability: likely to be many bugs in non-core library code, and faulty Lox input can crash the board
* Complete support for more "Arduino.h" functions
* Support for Arduino_GigaDisplayTouch
* Support for running scripts via a web interface (console-in-a-web-page)
* Use of the M4 co-processor as a graphics accelerator
* Changing the Lox interpreter language (not the JIT backend) to something less like JavaScript (GFX-Basic?)
* Adding to the ArduinoGraphics library (filled triangles, more fonts etc.)

If you have any other suggestions or ideas, please raise feature requests as issues.

## DIY

In the style of the book "Crafting Interpreters" by Bob Nystrom, which describes the creation of clox (and jlox) from scratch, here is a checklist of how to reproduce the work so far:

1. Clone (part of) the GitHub repo munificent/craftinginterpreters: `svn export https://github.com/munificent/craftinginterpreters.git/trunk/c`

2. Make a folder in your "libraries" directory and paste all of the files apart from `main.c` into it.

3. Change all occurences of `#include <stdio.h>` to `#include "clox_stdio.h"`

4. Add this code as `clox_stdio.h` to the folder:

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

5. Create a script which defines these two functions, and includes the necessary C headers (as a minimum). Here is a possible outline:

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
