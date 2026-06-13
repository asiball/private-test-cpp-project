// ─────────────────────────────────────────────────────────────
// I2cDriver 最小サンプル（レジスタ 1 本を読む生 I2C アクセス）
//
// open() → write_read()（レジスタポインタ書き込み + 値読み出しを 1 トランザクションで）
// → close() の最小フロー。高レベルの Ads1115 を介さず II2cDriver の生 API を直接使う例。
//
// 既定では ADS1115（0x48）の Config レジスタ(0x01)を 2 バイト読む。
// 実機（I2C にデバイスを接続）が必要。i2c-hal が揃う時のみビルドされる。
// ─────────────────────────────────────────────────────────────
#include "i2c_driver.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void usage(const char* prog)
{
    std::cout << "Usage: " << prog << " [options]\n"
              << "  -d, --device <path>  I2C device (default /dev/i2c-1)\n"
              << "  -a, --addr <hex>     7-bit slave address (default 0x48)\n"
              << "  -r, --reg <hex>      register pointer to read (default 0x01)\n"
              << "  -h, --help\n";
}

int parse_int(const std::string& s, const char* name)
{
    try {
        return std::stoi(s, nullptr, 0);   // 0x.. も受け付ける
    } catch (...) {
        std::cerr << name << ": invalid number '" << s << "'\n";
        std::exit(2);
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::string device = "/dev/i2c-1";
    uint16_t    addr   = 0x48;
    uint8_t     reg    = 0x01;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) { std::cerr << name << " requires an argument\n"; std::exit(2); }
            return argv[++i];
        };
        if (a == "-d" || a == "--device")      device = next("--device");
        else if (a == "-a" || a == "--addr")   addr   = static_cast<uint16_t>(parse_int(next("--addr"), "--addr"));
        else if (a == "-r" || a == "--reg")    reg    = static_cast<uint8_t>(parse_int(next("--reg"), "--reg"));
        else if (a == "-h" || a == "--help")  { usage(argv[0]); return 0; }
        else { std::cerr << "unknown option: " << a << "\n"; usage(argv[0]); return 2; }
    }

    embedded::I2cDriver bus(device);
    if (!bus.open(addr)) {
        std::cerr << "failed to open " << device << " addr 0x" << std::hex << addr
                  << " (errno=" << std::dec << bus.last_errno() << ")\n";
        return 1;
    }

    uint8_t rx[2] = {0, 0};
    if (bus.write_read(&reg, 1, rx, sizeof(rx)) < 0) {
        std::cerr << "write_read failed (errno=" << bus.last_errno() << ")\n";
        return 1;
    }

    uint16_t value = static_cast<uint16_t>((rx[0] << 8) | rx[1]);
    std::cout << "reg 0x" << std::hex << static_cast<int>(reg)
              << " = 0x" << value << std::dec << " (" << value << ")\n";

    bus.close();
    return 0;
}
