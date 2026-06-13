// ─────────────────────────────────────────────────────────────
// GPIO エッジ割り込みサンプル
//
// chip open → エッジ種別を設定して 1 本のラインを要求 → wait_event() で
// 割り込み（エッジ）を待ち、検知した時刻を出力する最小例。
// 「ポーリングせず割り込みで起こされる」イベント駆動 I/O を示す。
//
// 実機（GPIO ラインにスイッチや信号源を接続）が必要。gpio が揃う時のみビルドされる。
// ─────────────────────────────────────────────────────────────
#include "gpio_line.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void usage(const char* prog)
{
    std::cout << "Usage: " << prog << " [options]\n"
              << "      --chip <path>     GPIO chip (default /dev/gpiochip0)\n"
              << "      --line <offset>   GPIO line offset (required)\n"
              << "  -n, --count <N>       number of edges to wait for (default 5)\n"
              << "      --timeout <ms>    per-event timeout in ms (default 5000)\n"
              << "  -h, --help\n";
}

int parse_int(const std::string& s, const char* name)
{
    try {
        return std::stoi(s);
    } catch (...) {
        std::cerr << name << ": invalid number '" << s << "'\n";
        std::exit(2);
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::string chip    = "/dev/gpiochip0";
    int         line    = -1;
    int         count   = 5;
    int         timeout = 5000;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) { std::cerr << name << " requires an argument\n"; std::exit(2); }
            return argv[++i];
        };
        if (a == "--chip")                      chip    = next("--chip");
        else if (a == "--line")                 line    = parse_int(next("--line"), "--line");
        else if (a == "-n" || a == "--count")   count   = parse_int(next("--count"), "--count");
        else if (a == "--timeout")              timeout = parse_int(next("--timeout"), "--timeout");
        else if (a == "-h" || a == "--help")  { usage(argv[0]); return 0; }
        else { std::cerr << "unknown option: " << a << "\n"; usage(argv[0]); return 2; }
    }

    if (line < 0) {
        std::cerr << "--line <offset> is required\n";
        usage(argv[0]);
        return 2;
    }

    embedded::GpioLine gpio(chip, static_cast<unsigned>(line));
    if (!gpio.request_edge_events(embedded::GpioLine::Edge::Both)) {
        std::cerr << "failed to request edge events on " << chip << " line " << line
                  << " (errno=" << gpio.last_errno() << ")\n";
        return 1;
    }

    std::cout << "waiting for edges on " << chip << " line " << line << " ...\n";
    for (int i = 0; i < count; ++i) {
        int ev = gpio.wait_event(timeout);
        if (ev < 0) { std::cerr << "wait_event error (errno=" << gpio.last_errno() << ")\n"; break; }
        if (ev == 0) { std::cerr << "timeout (" << timeout << " ms)\n"; continue; }

        auto now = std::chrono::system_clock::now().time_since_epoch();
        auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        std::cout << "edge #" << (i + 1) << " at " << ms << " ms (epoch)\n";
    }

    gpio.close();
    return 0;
}
