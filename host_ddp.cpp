// main.cpp
// Minimal host driver that streams frames via DDP (UDP) using any Plugin.

#include "PixelDisplay.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
using socklen_t = int;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

// ------------------------
// Minimal Screen interface
// ------------------------

struct ScreenConfig {
  int width = 16;
  int height = 16;
  const char *ip = "192.168.1.112";
  uint16_t port = 4048;
  int fps = 20;
} cfg;

// A tiny "Screen" struct that your Plugin can call.
struct Screen : public PixelDisplay {
  ScreenConfig g_cfg;
  std::vector<uint8_t> g_rgb; // width*height*3
  int g_sock = -1;
  sockaddr_in g_addr{};
  std::chrono::steady_clock::time_point g_last;

  void init(const ScreenConfig &cfg) {
    g_cfg = cfg;
    g_rgb.assign(g_cfg.width * g_cfg.height * 3, 0);

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_sock < 0) {
      std::perror("socket");
      std::exit(1);
    }
    g_addr.sin_family = AF_INET;
    g_addr.sin_port = htons(g_cfg.port);
    g_addr.sin_addr.s_addr = inet_addr(g_cfg.ip);

    g_last = std::chrono::steady_clock::now();
  }

  inline int width() { return g_cfg.width; }
  inline int height() { return g_cfg.height; }

  void clear() { std::fill(g_rgb.begin(), g_rgb.end(), 0); }

  void setPixel(uint8_t x, uint8_t y, uint8_t type,
                uint8_t brightness) override {
    if (x < 0 || y < 0 || x >= g_cfg.width || y >= g_cfg.height)
      return;
    const int idx = (y * g_cfg.width + x) * 3;
    const uint8_t v = type ? brightness : 0;
    g_rgb[idx + 0] = v; // R
    g_rgb[idx + 1] = v; // G
    g_rgb[idx + 2] = v; // B
  }

  void present() {
    // pace frames
    if (g_cfg.fps > 0) {
      auto now = std::chrono::steady_clock::now();
      auto frame = std::chrono::milliseconds(1000 / g_cfg.fps);
      auto elapsed = now - g_last;
      if (elapsed < frame)
        std::this_thread::sleep_for(frame - elapsed);
      g_last = std::chrono::steady_clock::now();
    }

    // DDP header (10 bytes): 0x41, 0x00, then 8x 0x00
    std::vector<uint8_t> pkt;
    pkt.reserve(10 + g_rgb.size());
    pkt.push_back(0x41);
    pkt.push_back(0x00);
    for (int i = 0; i < 8; ++i)
      pkt.push_back(0x00);

    pkt.insert(pkt.end(), g_rgb.begin(), g_rgb.end());

    sendto(g_sock, reinterpret_cast<const char *>(pkt.data()), (int)pkt.size(),
           0, reinterpret_cast<sockaddr *>(&g_addr), sizeof(g_addr));
  }
} display;


struct DynamicJsonDocument {};
#include "include/plugins/Blop.h"
void Plugin::teardown() {}
void Plugin::websocketHook(DynamicJsonDocument&) {}
void Plugin::loop() {}  // default no-op
void Plugin::setId(int v) { id = v; }
int  Plugin::getId() const { return id; }


auto plugin = new BlobPlugin(display);

// ----------
// main()
// ----------
int main(int argc, char **argv) {

  display.init(cfg);
  plugin->setup();

  std::puts("Host driver running. Ctrl+C to quit.");
  while (true) {
    plugin->loop();    // plugin should draw with Screen::{clear,setPixel}
    display.present(); // send DDP packet
  }

  // (never reached)
  // p->teardown(); delete p;
  return 0;
}
