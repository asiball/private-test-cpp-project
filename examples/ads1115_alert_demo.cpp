// ─────────────────────────────────────────────────────────────
// ADS1115 + GPIO ALERT/RDY デモ
//
// 「ポーリング」と「割り込み（イベント駆動）」の対比を 1 本で見せる教材プログラム。
//
//   --gpiochip / --line を指定しない → ポーリング: sleep してから読む
//   --gpiochip / --line を指定する   → 割り込み: ALERT/RDY のエッジで起こされてから読む
//
// 実機（I2C に ADS1115、ALERT/RDY を GPIO に接続）が必要。
// 各コンポーネント（i2c-hal / libsensor(ads1115) / gpio）が揃っている時のみビルドされる。
// ─────────────────────────────────────────────────────────────
#include "ads1115.hpp"
#include "gpio_line.hpp"

#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

void usage(const char* prog)
{
    std::cout << "Usage: " << prog << " [options]\n"
              << "  -d, --device <path>   I2C device (default /dev/i2c-1)\n"
              << "  -c, --channel <0-3>   ADS1115 channel (default 0)\n"
              << "  -n, --count <N>       number of reads (default 5)\n"
              << "      --gpiochip <path> GPIO chip for ALERT/RDY (e.g. /dev/gpiochip0)\n"
              << "      --line <offset>   GPIO line offset for ALERT/RDY\n"
              << "  -h, --help\n"
              << "\n"
              << "ALERT/RDY ピン (--gpiochip + --line) を渡すと割り込み駆動、\n"
              << "渡さなければポーリング駆動で動作します。\n";
}

} // namespace

int main(int argc, char** argv)
{
    std::string device   = "/dev/i2c-1";
    uint8_t     channel  = 0;
    int         count    = 5;
    std::string gpiochip;
    int         line     = -1;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) { std::cerr << name << " requires an argument\n"; std::exit(2); }
            return argv[++i];
        };
        if (a == "-d" || a == "--device")      device  = next("--device");
        else if (a == "-c" || a == "--channel") channel = static_cast<uint8_t>(std::stoi(next("--channel")));
        else if (a == "-n" || a == "--count")   count   = std::stoi(next("--count"));
        else if (a == "--gpiochip")             gpiochip = next("--gpiochip");
        else if (a == "--line")                 line    = std::stoi(next("--line"));
        else if (a == "-h" || a == "--help")  { usage(argv[0]); return 0; }
        else { std::cerr << "unknown option: " << a << "\n"; usage(argv[0]); return 2; }
    }

    embedded::Ads1115 adc(device);
    if (!adc.open()) {
        std::cerr << "failed to open ADS1115 on " << device << "\n";
        return 1;
    }

    const bool interrupt_mode = !gpiochip.empty() && line >= 0;

    // 割り込みモード: ALERT/RDY を変換完了通知に構成し、GPIO エッジを要求する
    embedded::GpioLine alert(gpiochip, interrupt_mode ? static_cast<unsigned>(line) : 0);
    if (interrupt_mode) {
        if (!adc.enable_conversion_ready_pin()) {
            std::cerr << "failed to configure ALERT/RDY pin\n";
            return 1;
        }
        if (!alert.request_edge_events(embedded::GpioLine::Edge::Falling)) {
            std::cerr << "failed to request GPIO line " << line << " on " << gpiochip << "\n";
            return 1;
        }
        std::cout << "mode: interrupt (ALERT/RDY on " << gpiochip << " line " << line << ")\n";
    } else {
        std::cout << "mode: polling\n";
    }

    for (int i = 0; i < count; ++i) {
        if (interrupt_mode) {
            // 変換完了の割り込みで起こされるまで眠る（最大 1 秒）
            int ev = alert.wait_event(1000);
            if (ev < 0) { std::cerr << "wait_event error\n"; break; }
            if (ev == 0) { std::cerr << "timeout waiting for ALERT\n"; continue; }
        } else {
            // ポーリング: 変換時間ぶん待つ（128SPS なら ~8ms）
            usleep(10 * 1000);
        }

        auto v = adc.read_voltage(channel);
        if (v) {
            std::cout << "ch" << static_cast<int>(channel) << " = " << *v << " V\n";
        } else {
            std::cerr << "read failed\n";
        }
    }

    adc.close();
    return 0;
}
