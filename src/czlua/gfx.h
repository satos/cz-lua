#pragma once
// Drawing target abstraction for CZ Lua.
//
// All czlua.graphics primitives are implemented on top of a single
// setPixel() call, so any host (LVGL canvas on device, SDL on PC, the
// headless test) only needs to provide a pixel sink. Text is the one
// exception: it is host-rendered (LVGL font on device) because a
// framebuffer font is a v1 concern.

#include <algorithm>
#include <cstdint>
#include <vector>

namespace czlua {

struct IGfx {
  virtual ~IGfx() = default;
  virtual void clear(uint8_t r, uint8_t g, uint8_t b) = 0;
  virtual void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) = 0;
  virtual int width() const = 0;
  virtual int height() const = 0;
  // Host-rendered text. Default: no-op (headless / framebuffer hosts).
  virtual void text(int /*x*/, int /*y*/, const char * /*utf8*/) {}
};

// Software framebuffer, RGB565. On device this buffer is blitted to an
// LVGL canvas each frame; on PC it is what the headless test checksums.
class FramebufferGfx : public IGfx {
public:
  explicit FramebufferGfx(int w = 320, int h = 170)
      : w_(w), h_(h), buf_((size_t)w * (size_t)h, 0) {}

  static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((uint16_t)(r & 0xF8) << 8) |
                      ((uint16_t)(g & 0xFC) << 3) | (uint16_t)(b >> 3));
  }

  void clear(uint8_t r, uint8_t g, uint8_t b) override {
    std::fill(buf_.begin(), buf_.end(), rgb565(r, g, b));
  }

  void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) override {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    buf_[(size_t)y * (size_t)w_ + (size_t)x] = rgb565(r, g, b);
  }

  int width() const override { return w_; }
  int height() const override { return h_; }

  const uint16_t *pixels() const { return buf_.data(); }
  uint16_t *pixels() { return buf_.data(); }

  // FNV-1a over the buffer; used by the headless test to prove frames change.
  uint64_t checksum() const {
    uint64_t s = 1469598103934665603ULL;
    for (uint16_t p : buf_) {
      s ^= p;
      s *= 1099511628211ULL;
    }
    return s;
  }

private:
  int w_, h_;
  std::vector<uint16_t> buf_;
};

} // namespace czlua
