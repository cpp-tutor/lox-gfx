#ifndef clox_gfx_h
#define clox_gfx_h

#include "value.h"

Value gfx_millis();
Value gfx_micros();
Value gfx_delay(unsigned long ms);
Value gfx_delayMicroseconds(unsigned us);
Value gfx_pinMode(int pin, const char *mode_c_str);
Value gfx_digitalWrite(int pin, bool level);
Value gfx_digitalRead(int pin);
Value gfx_analogWriteResolution(int bits);
Value gfx_analogWrite(int pin, int level);
Value gfx_analogReadResolution(int bits);
Value gfx_analogRead(int pin);
Value gfx_analogReference(uint8_t r);
Value gfx_bit(unsigned b);
Value gfx_bitClear(unsigned long n, unsigned b);
Value gfx_bitRead(unsigned long n, unsigned b);
Value gfx_bitSet(unsigned long n, unsigned b);
Value gfx_highByte(unsigned long n);
Value gfx_lowByte(unsigned long n);
Value gfx_isRotated();
Value gfx_beginDraw();
Value gfx_endDraw();
Value gfx_width();
Value gfx_height();
Value gfx_fill(int r, int g, int b);
Value gfx_noFill();
Value gfx_stroke(int r, int g, int b);
Value gfx_noStroke();
Value gfx_background(int r, int g, int b);
Value gfx_clear();
Value gfx_circle(int x, int y, int diameter);
Value gfx_ellipse(int x, int y, int width, int height);
Value gfx_line(int x1, int y1, int x2, int y2);
Value gfx_point(int x, int y);
Value gfx_rect(int x, int y, int width, int height);
Value gfx_text(const char *text_c_str, int x, int y);
Value gfx_textFont(const char *font_c_str);
Value gfx_textFontWidth();
Value gfx_textFontHeight();
Value gfx_beginText(int x, int y, int r, int g, int b);
Value gfx_endText(const char *scroll_type_c_str);
Value gfx_textScrollSpeed(unsigned long speed);
Value gfx_printStr(const char *text);
Value gfx_printStrLn(const char *text);
Value gfx_printInt(long long num, int base);
Value gfx_printFloat(double num, int digits);

#define GFX_DECLARE(type) \
  defineNative(#type, gfx##type##Native)

#define GFX_ARGS_0
#define GFX_ARGS_1 AS_NUMBER(args[0])
#define GFX_ARGS_2 AS_NUMBER(args[0]), AS_NUMBER(args[1])
#define GFX_ARGS_3 AS_NUMBER(args[0]), AS_NUMBER(args[1]), AS_NUMBER(args[2])
#define GFX_ARGS_4 AS_NUMBER(args[0]), AS_NUMBER(args[1]), AS_NUMBER(args[2]), AS_NUMBER(args[3])
#define GFX_ARGS_5 AS_NUMBER(args[0]), AS_NUMBER(args[1]), AS_NUMBER(args[2]), AS_NUMBER(args[3]), AS_NUMBER(args[4])

#define GFX_DEFINE(type, arity) \
  static Value gfx##type##Native(int argCount, Value *args) { \
    if (arity != argCount) { \
      runtimeError("Expected %d arguments but got %d in call to %s().", \
        arity, argCount, #type); \
      return ERR_VAL; \
    } \
    for (int arg = 0; arg != argCount; ++arg) { \
      if (!IS_NUMBER(args[arg])) { \
        runtimeError("Expected a number for argument %d call to %s().", \
          arg + 1, #type); \
        return ERR_VAL; \
      } \
    } \
    return gfx_##type(GFX_ARGS_##arity); \
  }

#define GFX_ARGS_NUM_STR AS_NUMBER(args[0]), AS_CSTRING(args[1])

#define GFX_DEFINE_NUM_STR(type) \
  static Value gfx##type##Native(int argCount, Value *args) { \
    if (2 != argCount) { \
      runtimeError("Expected %d arguments but got %d in call to %s().", \
        2, argCount, #type); \
      return ERR_VAL; \
    } \
    if (!IS_NUMBER(args[0])) { \
      runtimeError("Expected a number for argument %d call to %s().", \
        1, #type); \
      return ERR_VAL; \
    } \
    if (!IS_STRING(args[1])) { \
      runtimeError("Expected a string for argument %d call to %s().", \
        2, #type); \
      return ERR_VAL; \
    } \
    return gfx_##type(GFX_ARGS_NUM_STR); \
  }

#define GFX_ARGS_NUM_BOOL AS_NUMBER(args[0]), AS_BOOL(args[1])

#define GFX_DEFINE_NUM_BOOL(type) \
  static Value gfx##type##Native(int argCount, Value *args) { \
    if (2 != argCount) { \
      runtimeError("Expected %d arguments but got %d in call to %s().", \
        2, argCount, #type); \
      return ERR_VAL; \
    } \
    if (!IS_NUMBER(args[0])) { \
      runtimeError("Expected a number for argument %d call to %s().", \
        1, #type); \
      return ERR_VAL; \
    } \
    if (!IS_BOOL(args[1])) { \
      runtimeError("Expected a Boolean for argument %d call to %s().", \
        2, #type); \
      return ERR_VAL; \
    } \
    return gfx_##type(GFX_ARGS_NUM_BOOL); \
  }

#define GFX_ARGS_STR_NUM_NUM AS_CSTRING(args[0]), AS_NUMBER(args[1]), AS_NUMBER(args[2])

#define GFX_DEFINE_STR_NUM_NUM(type) \
  static Value gfx##type##Native(int argCount, Value *args) { \
    if (3 != argCount) { \
      runtimeError("Expected %d arguments but got %d in call to %s().", \
        3, argCount, #type); \
      return ERR_VAL; \
    } \
    if (!IS_STRING(args[0])) { \
      runtimeError("Expected a string for argument %d call to %s().", \
        1, #type); \
      return ERR_VAL; \
    } \
    if (!IS_NUMBER(args[1])) { \
      runtimeError("Expected a number for argument %d call to %s().", \
        2, #type); \
      return ERR_VAL; \
    } \
    if (!IS_NUMBER(args[2])) { \
      runtimeError("Expected a number for argument %d call to %s().", \
        3, #type); \
        return ERR_VAL; \
    } \
    return gfx_##type(GFX_ARGS_STR_NUM_NUM); \
  }

#define GFX_ARGS_STR AS_CSTRING(args[0])

#define GFX_DEFINE_STR(type) \
  static Value gfx##type##Native(int argCount, Value *args) { \
    if (1 != argCount) { \
      runtimeError("Expected %d arguments but got %d in call to %s().", \
        1, argCount, #type); \
      return ERR_VAL; \
    } \
    if (!IS_STRING(args[0])) { \
      runtimeError("Expected a string for argument %d call to %s().", \
        1, #type); \
      return ERR_VAL; \
    } \
    return gfx_##type(GFX_ARGS_STR); \
  }

  #endif