#include <AnimatedGIF.h>
#include <Adafruit_NeoPixel.h>
#include <FS.h>
#include <LittleFS.h>

// Pin configuration
#define PIN 6
#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8
#define NUMPIXELS (MATRIX_WIDTH * MATRIX_HEIGHT)

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
AnimatedGIF gif;

#define BRIGHT_SHIFT 3
#define MAX_GIF_SIZE 32768
#define MAX_FILENAME_LEN 32

uint8_t *gifBuffer = nullptr;
size_t gifSize = 0;
char currentGifFile[MAX_FILENAME_LEN] = "/105.gif";  // Default GIF file
bool needToLoadNewGif = true;

uint16_t XY(uint8_t x, uint8_t y) {
  return (y * MATRIX_WIDTH) + x;
}


void GIFDraw(GIFDRAW *pDraw) {
  uint8_t r, g, b, *s, *p, *pPal = (uint8_t *)pDraw->pPalette;
  int x, y = pDraw->iY + pDraw->y;

  if (y >= MATRIX_HEIGHT) return;

  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) {
    p = &pPal[pDraw->ucBackground * 3];
    r = p[0] >> BRIGHT_SHIFT;
    g = p[1] >> BRIGHT_SHIFT;
    b = p[2] >> BRIGHT_SHIFT;

    for (x = 0; x < pDraw->iWidth && x < MATRIX_WIDTH; x++) {
      if (s[x] == pDraw->ucTransparent) {
        pixels.setPixelColor(XY(x, y), pixels.Color(r, g, b));
      }
    }
    pDraw->ucHasTransparency = 0;
  }

  if (pDraw->ucHasTransparency) {
    const uint8_t ucTransparent = pDraw->ucTransparent;
    for (x = 0; x < pDraw->iWidth && x < MATRIX_WIDTH; x++) {
      if (s[x] != ucTransparent) {
        p = &pPal[s[x] * 3];
        pixels.setPixelColor(XY(x, y), pixels.Color(p[0] >> BRIGHT_SHIFT, p[1] >> BRIGHT_SHIFT, p[2] >> BRIGHT_SHIFT));
      }
    }
  } else {
    for (x = 0; x < pDraw->iWidth && x < MATRIX_WIDTH; x++) {
      p = &pPal[s[x] * 3];
      pixels.setPixelColor(XY(x, y), pixels.Color(p[0] >> BRIGHT_SHIFT, p[1] >> BRIGHT_SHIFT, p[2] >> BRIGHT_SHIFT));
    }
  }

  if (pDraw->y == pDraw->iHeight - 1) {
    pixels.show();
  }
}

bool loadGifFile() {
  File file = LittleFS.open(currentGifFile, "r");
  if (!file) {
    Serial.printf("Failed to open %s\n", currentGifFile);
    return false;
  }

  gifSize = file.read(gifBuffer, MAX_GIF_SIZE);
  file.close();

  if (gifSize == 0) {
    Serial.println("Failed to read file");
    return false;
  }

  return true;
}

void checkSerial() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.startsWith("gif:")) {
      String filename = command.substring(4);
      filename.trim();

      // Add leading slash if not present
      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }

      // Add .gif extension if not present
      if (!filename.endsWith(".gif")) {
        filename += ".gif";
      }

      // Check if file exists
      if (LittleFS.exists(filename)) {
        strncpy(currentGifFile, filename.c_str(), MAX_FILENAME_LEN - 1);
        currentGifFile[MAX_FILENAME_LEN - 1] = '\0';
        needToLoadNewGif = true;
        Serial.printf("Switching to %s\n", currentGifFile);
      } else {
        Serial.printf("File %s not found\n", filename.c_str());
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32 GIF Player");
  Serial.println("Send 'gif:filename' to change GIF (e.g., 'gif:2' for /2.gif)");

  pixels.begin();
  pixels.clear();
  pixels.show();

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  gifBuffer = (uint8_t *)malloc(MAX_GIF_SIZE);
  if (!gifBuffer) {
    Serial.println("Failed to allocate GIF buffer");
    return;
  }

  gif.begin(GIF_PALETTE_RGB888);
}

void loop() {
  checkSerial();

  if (needToLoadNewGif) {
    if (loadGifFile()) {
      needToLoadNewGif = false;
      gif.close();  // Close any previously opened GIF
    } else {
      delay(1000);
      return;
    }
  }

  int rc = gif.open(gifBuffer, gifSize, GIFDraw);
  if (rc) {
    while (rc && !needToLoadNewGif) {
      checkSerial();  // Check for new commands between frames
      rc = gif.playFrame(true, NULL);
      if (!rc) {
        // End of animation reached, reopen for looping
        gif.close();
        rc = gif.open(gifBuffer, gifSize, GIFDraw);
      }
    }
    gif.close();
  } else {
    Serial.println("Failed to open GIF");
    delay(1000);
  }
}