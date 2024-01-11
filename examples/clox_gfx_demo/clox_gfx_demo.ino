#include <ArduinoGraphics.h>
#include <Arduino_H7_Video.h>
#include <FATFileSystem.h>
#include <Arduino_USBHostMbed5.h>

extern "C" {
#include <stdarg.h>
#include "vm.h"
#include "clox_stdio.h"
#include "clox_gfx.h"
}

Arduino_H7_Video Display(800, 480, GigaDisplayShield);

USBHostMSD msd;
mbed::FATFileSystem usb("usb");

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    delay(100);
  }

  pinMode(LEDB, OUTPUT);
  digitalWrite(LEDB, LOW);
  Serial_printf("Initializing clox-gfx VM...\n");
  Display.begin();
  Display.beginDraw();
  Display.background(0, 0, 0);
  Display.clear();
  Display.endDraw();
  delay(1000);
  initVM();
  digitalWrite(LEDB, HIGH);
}

void loop() {
  Serial_printf("> ");
  String input, line;
  while (line != "\n" && !line.startsWith("load")) {
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
    if (line != "\n" && !line.startsWith("load")) {
      Serial_printf(". ");
    }
  }

  digitalWrite(LEDB, LOW);
  if (line.startsWith("load")) {
    String filename = line.substring(line.indexOf('\"'), line.lastIndexOf('\"'));
    if (filename.length()) {
      interpret(readFile(filename).c_str());
    }
    else {
      Serial_printf("%s\n", "Syntax: load \"my_script.lox\"");
    }
  }
  else {
    interpret(input.c_str());
  }
  digitalWrite(LEDB, HIGH);
}

String readFile(String filename) {
  Serial_printf("Mounting USB device...\n");
  int err = usb.mount(&msd);
  if (err) {
    Serial_printf("Error mounting USB device: %d\n", err);
  }
  else {
    filename = String("/usb/") + filename;
    FILE *f = fopen(filename.c_str(), "r+");
    String fileData;
    char buf[256];
    if (f) {
      while (fgets(buf, 256, f) != NULL) {
        fileData += String(buf);
      }
      fclose(f);
      return fileData;
    }
    else {
      Serial_printf("Error reading file: %s\n", filename);
    }
  }
  return "";
}

extern "C" {

int Serial_printf(const char *fmt, ...) {
  if (Serial) {
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);
    int nchars = vsnprintf(nullptr, 0, fmt, args2);
    va_end(args2);
    char *buf = (char *)malloc(nchars + 1);
    vsprintf(buf, fmt, args);
    va_end(args);
    Serial.print(buf);
    free(buf);
    Serial.flush();
    return nchars;
  }
  else {
    return 0;
  }
}

int Serial_fprintf(FILE *dummy, const char *fmt, ...) {
  if (Serial) {
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);
    int nchars = vsnprintf(nullptr, 0, fmt, args2);
    va_end(args2);
    char *buf = (char *)malloc(nchars + 1);
    vsprintf(buf, fmt, args);
    va_end(args);
    Serial.print(buf);
    free(buf);
    Serial.flush();
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

#define GFX_RETURN Value
#define GFX_RETURN_NIL NIL_VAL
#define GFX_RETURN_NUM(n) NUMBER_VAL(n)
#define GFX_RETURN_BOOL(b) BOOL_VAL(b)

GFX_RETURN gfx_millis() {
  return GFX_RETURN_NUM(millis());
}

GFX_RETURN gfx_micros() {
  return GFX_RETURN_NUM(micros());
}

GFX_RETURN gfx_delay(unsigned long ms) {
  delay(ms);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_delayMicroseconds(unsigned us) {
  delayMicroseconds(us);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_pinMode(int pin, const char *mode_c_str) {
  String mode = mode_c_str;
  if (mode == "INPUT") {
    pinMode(pin, INPUT);
  }
  else if (mode == "OUTPUT") {
    pinMode(pin, OUTPUT);
  }
  else if (mode == "INPUT_PULLUP") {
    pinMode(pin, INPUT_PULLUP);
  }
  else {
    runtimeError("Bad mode to pinMode(): Must be one of \"INPUT\", \"OUTPUT\" or \"INPUT_PULLUP\"");
  }
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_digitalWrite(int pin, bool level) {
  digitalWrite(pin, level ? HIGH : LOW);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_digitalRead(int pin) {
  return GFX_RETURN_BOOL((digitalRead(pin) == HIGH) ? true : false);
}

GFX_RETURN gfx_analogWriteResolution(int bits) {
  analogWriteResolution(bits);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_analogWrite(int pin, int level) {
  analogWrite(pin, level);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_analogReadResolution(int bits) {
  analogReadResolution(bits);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_analogRead(int pin) {
  return GFX_RETURN_NUM(analogRead(pin));
}

GFX_RETURN gfx_analogReference(uint8_t r) {
  //analogReference(r);
  runtimeError("Bad call to analogReference(): Not implemented on GIGA");
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_bit(unsigned b) {
  return GFX_RETURN_NUM(bit(b));
}

GFX_RETURN gfx_bitClear(unsigned long n, unsigned b) {
  return GFX_RETURN_NUM(bitClear(n, b));
}

GFX_RETURN gfx_bitRead(unsigned long n, unsigned b) {
  return GFX_RETURN_NUM(bitRead(n, b));
}

// bitWrite() not used as it requires reference parameter, use: v = bitSet(n, b); or v = bitClear(n, b);

GFX_RETURN gfx_bitSet(unsigned long n, unsigned b) {
  return GFX_RETURN_NUM(bitRead(n, b));
}

GFX_RETURN gfx_highByte(unsigned long n) {
  return GFX_RETURN_NUM(highByte(n));
}

GFX_RETURN gfx_lowByte(unsigned long n) {
  return GFX_RETURN_NUM(lowByte(n));
}

GFX_RETURN gfx_isRotated() {
  return GFX_RETURN_BOOL(Display.isRotated());
}

GFX_RETURN gfx_beginDraw() {
  Display.beginDraw();
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_endDraw() {
  Display.endDraw();
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_width() {
  return GFX_RETURN_NUM(Display.width());
}

GFX_RETURN gfx_height() {
  return GFX_RETURN_NUM(Display.height());
}

GFX_RETURN gfx_fill(int r, int g, int b) {
  Display.fill(r, g, b);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_noFill() {
  Display.noFill();
  return GFX_RETURN_NIL;
}
GFX_RETURN gfx_stroke(int r, int g, int b) {
  Display.stroke(r, g, b);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_noStroke() {
  Display.noStroke();
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_background(int r, int g, int b) {
  Display.background(r, g, b);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_clear() {
  Display.clear();
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_circle(int x, int y, int diameter) {
  Display.circle(x, y, diameter);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_ellipse(int x, int y, int width, int height) {
  Display.ellipse(x, y, width, height);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_line(int x1, int y1, int x2, int y2) {
  Display.line(x1, y1, x2, y2);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_point(int x, int y) {
  Display.point(x, y);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_rect(int x, int y, int width, int height) {
  Display.rect(x, y, width, height);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_text(const char *text_c_str, int x, int y) {
  Display.text(text_c_str, x, y);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_textFont(const char *font_c_str) {
  String font = font_c_str;
  if (font == "4x6") {
    Display.textFont(Font_4x6);
  }
  else if (font == "5x7") {
    Display.textFont(Font_5x7);
  }
  else {
    runtimeError("Bad font to textFont(): Must be one of \"4x6\" or \"5x7\"");
  }
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_textFontWidth() {
  return GFX_RETURN_NUM(Display.textFontWidth());
}

GFX_RETURN gfx_textFontHeight() {
  return GFX_RETURN_NUM(Display.textFontHeight());
}

GFX_RETURN gfx_beginText(int x, int y, int r, int g, int b) {
  Display.beginText(x, y, r, g, b);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_endText(const char *scroll_type_c_str) {
  String scroll_type = scroll_type_c_str;

  int scrolling = NO_SCROLL;
  if (scroll_type == "LEFT") {
    scrolling = SCROLL_LEFT;
  }
  else if (scroll_type == "RIGHT") {
    scrolling = SCROLL_RIGHT;
  }
  else if (scroll_type == "UP") {
    scrolling = SCROLL_UP;
  }
  else if (scroll_type == "DOWN") {
    scrolling = SCROLL_DOWN;
  }

  Display.endText(scrolling);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_textScrollSpeed(unsigned long speed) {
  Display.textScrollSpeed(speed);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_printStr(const char *text) {
  Display.print(text);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_printStrLn(const char *text) {
  Display.println(text);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_printInt(long long num, int base) {
  Display.print(num, base);
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_printFloat(double num, int digits) {
  Display.print(num, digits);
  return GFX_RETURN_NIL;
}

} // extern "C"