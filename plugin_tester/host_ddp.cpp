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
#include <numeric>
#include <algorithm>
#include <iostream>

#ifdef _WIN32
#include <conio.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
using socklen_t = int;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>
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
  long g_packets_sent = 0;

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

    int res = sendto(g_sock, reinterpret_cast<const char *>(pkt.data()), (int)pkt.size(),
           0, reinterpret_cast<sockaddr *>(&g_addr), sizeof(g_addr));
    if (res >= 0) {
      g_packets_sent++;
    }
  }
} display;


#include "Plugin.h"

#define QUOTE(str) #str
#define INCLUDE_FILE(str) QUOTE(str)

#ifdef PLUGIN_HEADER_PATH
#include INCLUDE_FILE(PLUGIN_HEADER_PATH)
#else
#error "PLUGIN_HEADER_PATH is not defined. Please define it in your build system."
#endif


#ifdef PLUGIN_CLASS_NAME
auto plugin = new PLUGIN_CLASS_NAME(display);
#else
#error "PLUGIN_CLASS_NAME is not defined. Please define it in your build system."
#endif

// ----------
// main()
// ----------
int main(int argc, char **argv) {

  display.init(cfg);
  plugin->setup();

  std::puts("Host driver running. Press any key to exit.");

  std::vector<double> frame_times_ms;
  frame_times_ms.reserve(100000); // Avoid reallocations

  auto start_time = std::chrono::steady_clock::now();
  auto last_report_time = start_time;
  long frame_count_since_report = 0;

  while (true) {
    auto frame_start = std::chrono::steady_clock::now();

    plugin->loop();    // plugin should draw with Screen::{clear,setPixel}
    display.present(); // send DDP packet

    auto frame_end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> frame_duration =
        frame_end - frame_start;
    frame_times_ms.push_back(frame_duration.count());
    frame_count_since_report++;

    auto now = std::chrono::steady_clock::now();
    if (now - last_report_time >= std::chrono::seconds(1)) {
      double elapsed_s =
          std::chrono::duration<double>(now - last_report_time).count();
      double fps = frame_count_since_report / elapsed_s;
      // Using \r to update the line in place
      std::printf("\rFPS: %6.2f, Packets Sent: %-10ld", fps,
                  display.g_packets_sent);
      std::fflush(stdout);

      last_report_time = now;
      frame_count_since_report = 0;
    }
  }

  std::puts("\n\nExiting. Final stats:");

  if (frame_times_ms.empty()) {
    std::puts("No frames rendered.");
    return 0;
  }

  std::sort(frame_times_ms.begin(), frame_times_ms.end());
  long total_frames = frame_times_ms.size();
  double total_time_s = std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - start_time)
                            .count();

  double avg_fps = total_frames / total_time_s;

  double sum_frame_time =
      std::accumulate(frame_times_ms.begin(), frame_times_ms.end(), 0.0);
  double avg_frame_time = sum_frame_time / total_frames;
  double min_frame_time = frame_times_ms.front();
  double max_frame_time = frame_times_ms.back();

  auto percentile = [&](double p) {
    return frame_times_ms[static_cast<size_t>(total_frames * p)];
  };

  std::printf("Total frames: %ld\n", total_frames);
  std::printf("Total packets sent: %ld\n", display.g_packets_sent);
  std::printf("Total time: %.2f s\n", total_time_s);
  std::printf("Average FPS: %.2f\n", avg_fps);
  std::printf(
      "Frame time (ms) - Avg: %.2f, Min: %.2f (%.1f FPS), Max: %.2f (%.1f FPS)\n",
      avg_frame_time, min_frame_time, 1000.0 / min_frame_time, max_frame_time,
      1000.0 / max_frame_time);

  std::printf("Frame time (ms) percentiles:\n");
  std::printf("  50th (median): %.2f\n", percentile(0.5));
  std::printf("  90th:          %.2f\n", percentile(0.90));
  std::printf("  95th:          %.2f\n", percentile(0.95));
  std::printf("  99th:          %.2f\n", percentile(0.99));

  return 0;
}
