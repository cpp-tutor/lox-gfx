extern "C" {
#include "vm.h"
#include "memory.h"
#include "clox_stdio.h"
#include "clox_gfx.h"
}

#include "clox_gfx_config.h"
#if CLOX_USE_SDRAM
#include <SDRAM.h>
#endif

#if CLOX_GRAPHICS
#define NEED_SDRAM_BEGIN 0
#include <ArduinoGraphics.h>
#include <Arduino_H7_Video.h>

#if ( defined(ARDUINO_GIGA) && defined(ARDUINO_ARCH_MBED) )
Arduino_H7_Video Display(800, 480, GigaDisplayShield);
#elif ( defined(ARDUINO_PORTENTA_H7_M7) && defined(ARDUINO_ARCH_MBED) )
Arduino_H7_Video Display(1024, 768, USBCVideo);
#else
#error Graphics only available on GIGA R1 (GigaDisplayShield) and Portenta H7 (USBCVideo)
#endif

#else
#define NEED_SDRAM_BEGIN 1
#endif

#if CLOX_USB_HOST
#include <FATFileSystem.h>
#include <Arduino_USBHostMbed5.h>

USBHostMSD msd;
mbed::FATFileSystem usb("usb");
#endif

#if CLOX_WEB_CONSOLE
#if ( defined(ARDUINO_GIGA) && defined(ARDUINO_ARCH_MBED) )
#define WEBSOCKETS_USE_GIGA_R1_WIFI 1
#elif ( defined(ARDUINO_PORTENTA_H7_M7) && defined(ARDUINO_ARCH_MBED) )
#define WEBSOCKETS_USE_PORTENTA_H7_WIFI 1
#else
#error Unsupported network device for WebSockets
#endif

#include <WiFi.h>
#include <WebSockets2_Generic.h>
#include "arduino_secrets.h"
#include "web_console.h"

const char ssid[] = SECRET_SSID, password[] = SECRET_PASS;
const uint16_t websockets_server_port = 8080;
String ip_address, inputbuf, outputbuf;

WiFiServer webserver(80);

using namespace websockets2_generic;

WebsocketsServer server;
WebsocketsClient client;
WebsocketsMessage msg;
#endif

void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 5000L) {
    delay(100);
  }

#if CLOX_WEB_CONSOLE
  Serial.print("Connecting to ");
  Serial.print(ssid);

  while (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, password);
      Serial.print(".");
      delay(2000);
  }

  Serial.print("\nConnected to ");
  Serial.println(ssid);

  IPAddress ip = WiFi.localIP();
  ip_address = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  server.listen(websockets_server_port);
  Serial.print("http://");
  Serial.println(ip_address);
  webserver.begin();
#endif

  pinMode(LEDB, OUTPUT);
  digitalWrite(LEDB, LOW);
  Serial_printf("Initializing clox-gfx VM...\n");
#if CLOX_GRAPHICS
  Display.begin();
  Display.beginDraw();
  Display.background(0, 0, 0);
  Display.clear();
  Display.endDraw();
#endif
  delay(1000);
#if CLOX_USE_SDRAM && NEED_SDRAM_BEGIN
  SDRAM.begin();
#endif
  initVM();
  digitalWrite(LEDB, HIGH);
}

void loop() {
  Serial_printf("> ");
  String input, line;
#if CLOX_WEB_CONSOLE
  String console_buffer;
#endif
  while (line != "\n" && !line.startsWith("load")) {
    line = "";
    char ch = '\0';
    while (ch != '\n') {
      if (Serial.available()) {
        ch = Serial.read();
        line += ch;
      }
#if CLOX_WEB_CONSOLE
      if (!console_buffer.length()) {
        if (!client.available()) {
          client = server.accept();
          if (poll_webserver(20)) {
            poll_webserver(1000);  // wait for WebSocket connection to server
            Serial_printf("Connected to clox-gfx!\n> ");
          }
        }
        else if (!console_buffer.length()) {
          WebsocketsMessage msg = client.readNonBlocking();
          console_buffer = msg.data();
          poll_webserver(50);
        }
      }
      if (console_buffer.length()) {
        ch = console_buffer.charAt(0);
        line += ch;
        console_buffer = console_buffer.substring(1);
      }
#endif
    }
    input += line;
    if (Serial) {
      Serial.print(line.c_str()); // note: do not echo to Web Terminal
    }
    if (line != "\n" && !line.startsWith("load")) {
      Serial_printf(". ");
    }
  }

  digitalWrite(LEDB, LOW);
  if (line.startsWith("load")) {
    String filename = line.substring(line.indexOf('\"'), line.lastIndexOf('\"'));
    if (filename.length()) {
      String program = readFile(filename);
      if (program.length()) {
        interpret(program.c_str());
      }
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
#if CLOX_USB_HOST
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
#else
  Serial_printf("Error: USB support not available.\n");
#endif
  return "";
}

#if CLOX_WEB_CONSOLE
int poll_webserver(unsigned long poll_time) {
  unsigned long until = millis() + poll_time;
  while (millis() < until) {
    WiFiClient webclient = webserver.available();
    if (webclient) {
      boolean currentLineIsBlank = true;
      while (webclient.connected()) {
        if (webclient.available()) {
          char c = webclient.read();
          if (c == '\n' && currentLineIsBlank) {
            webclient.println("HTTP/1.1 200 OK");
            webclient.println("Content-Type: text/html");
            webclient.println("Connection: close");  // the connection will be closed after completion of the response
            webclient.println();
            String ip_and_port = ip_address + ":" + String(websockets_server_port), html = web_console;
            html.replace("%LOCAL_ADDRESS_AND_PORT%", ip_and_port);
            webclient.println(html);
            Serial.println("Page served.");
            break;
          }
          if (c == '\n') {
            currentLineIsBlank = true;
          } else if (c != '\r') {
            currentLineIsBlank = false;
          }
        }
      }
      // give the web browser time to receive the data
      delay(10);
      webclient.stop();
      return 1;
    }
    delay(20);
  }
  return 0;
}
#endif

extern "C" {

#if CLOX_USE_SDRAM
void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  vm.bytesAllocated += newSize - oldSize;

  if (newSize > oldSize) {
    if (vm.bytesAllocated > vm.nextGC) {
      collectGarbage();
    }
  }

  if (newSize == 0) {
    SDRAM.free(pointer);
    return NULL;
  }

  void* result = SDRAM.malloc(newSize);
  if (result == NULL) exit(1);
  memcpy(result, pointer, (oldSize < newSize) ? oldSize : newSize);
  SDRAM.free(pointer);
  return result;
}
#else
void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  vm.bytesAllocated += newSize - oldSize;

  if (newSize > oldSize) {
    if (vm.bytesAllocated > vm.nextGC) {
      collectGarbage();
    }
  }

  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}
#endif

int Serial_printf(const char *fmt, ...) {
  if (Serial || CLOX_WEB_CONSOLE) {
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
  if (Serial || CLOX_WEB_CONSOLE) {
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
  va_list(args2);
  va_copy(args2, args);
  int nchars = vsnprintf(nullptr, 0, fmt, args2);
  va_end(args2);
  char *buf = (char *)malloc(nchars + 1);
  vsprintf(buf, fmt, args);
  if (Serial) {
    Serial.print(buf);
  }
#if CLOX_WEB_CONSOLE
  if (client.available()) {
    client.send(buf, nchars);
  }
#endif
  free(buf);
  Serial.flush();
  return (Serial || CLOX_WEB_CONSOLE) ? nchars : 0;
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
#if CLOX_GRAPHICS
  return GFX_RETURN_BOOL(Display.isRotated());
#else
  return GFX_RETURN_NIL;
#endif
}

GFX_RETURN gfx_beginDraw() {
#if CLOX_GRAPHICS
  Display.beginDraw();
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_endDraw() {
#if CLOX_GRAPHICS
  Display.endDraw();
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_width() {
#if CLOX_GRAPHICS
  return GFX_RETURN_NUM(Display.width());
#else
  return GFX_RETURN_NIL;
#endif
}

GFX_RETURN gfx_height() {
#if CLOX_GRAPHICS
  return GFX_RETURN_NUM(Display.height());
#else
  return GFX_RETURN_NIL;
#endif
}

GFX_RETURN gfx_fill(int r, int g, int b) {
#if CLOX_GRAPHICS
  Display.fill(r, g, b);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_noFill() {
#if CLOX_GRAPHICS
  Display.noFill();
#endif
  return GFX_RETURN_NIL;
}
GFX_RETURN gfx_stroke(int r, int g, int b) {
#if CLOX_GRAPHICS
  Display.stroke(r, g, b);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_noStroke() {
#if CLOX_GRAPHICS
  Display.noStroke();
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_background(int r, int g, int b) {
#if CLOX_GRAPHICS
  Display.background(r, g, b);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_clear() {
#if CLOX_GRAPHICS
  Display.clear();
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_circle(int x, int y, int diameter) {
#if CLOX_GRAPHICS
  Display.circle(x, y, diameter);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_ellipse(int x, int y, int width, int height) {
#if CLOX_GRAPHICS
  Display.ellipse(x, y, width, height);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_line(int x1, int y1, int x2, int y2) {
#if CLOX_GRAPHICS
  Display.line(x1, y1, x2, y2);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_point(int x, int y) {
#if CLOX_GRAPHICS
  Display.point(x, y);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_rect(int x, int y, int width, int height) {
#if CLOX_GRAPHICS
  Display.rect(x, y, width, height);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_text(const char *text_c_str, int x, int y) {
#if CLOX_GRAPHICS
  Display.text(text_c_str, x, y);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_textFont(const char *font_c_str) {
#if CLOX_GRAPHICS
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
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_textFontWidth() {
#if CLOX_GRAPHICS
  return GFX_RETURN_NUM(Display.textFontWidth());
#else
  return GFX_RETURN_NIL;
#endif
}

GFX_RETURN gfx_textFontHeight() {
#if CLOX_GRAPHICS
  return GFX_RETURN_NUM(Display.textFontHeight());
#else
  return GFX_RETURN_NIL;
#endif
}

GFX_RETURN gfx_beginText(int x, int y, int r, int g, int b) {
#if CLOX_GRAPHICS
  Display.beginText(x, y, r, g, b);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_endText(const char *scroll_type_c_str) {
#if CLOX_GRAPHICS
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
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_textScrollSpeed(unsigned long speed) {
#if CLOX_GRAPHICS
  Display.textScrollSpeed(speed);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_printStr(const char *text) {
#if CLOX_GRAPHICS
  Display.print(text);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_printStrLn(const char *text) {
#if CLOX_GRAPHICS
  Display.println(text);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_printInt(long long num, int base) {
#if CLOX_GRAPHICS
  Display.print(num, base);
#endif
  return GFX_RETURN_NIL;
}

GFX_RETURN gfx_printFloat(double num, int digits) {
#if CLOX_GRAPHICS
  Display.print(num, digits);
#endif
  return GFX_RETURN_NIL;
}

} // extern "C"