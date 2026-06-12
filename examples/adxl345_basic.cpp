// ─────────────────────────────────────────────────────────────
// ADXL345（SPI 加速度センサ）基本サンプル
//
// 初期化 → read_g() ポーリング → 加速度を CSV 形式で標準出力に出す最小例。
// レジスタ型デバイス（libadxl345）の「開く→繰り返し読む→閉じる」フローを示す。
//
// 実機（SPI に ADXL345 を接続）が必要。libadxl345 + spi-hal が揃う時のみビルドされる。
// ─────────────────────────────────────────────────────────────
#include "adxl345.hpp"

#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void usage(const char* prog)
{
    std::cout << "Usage: " << prog << " [options]\n"
              << "  -d, --device <path>  SPI device (default /dev/spidev0.0)\n"
              << "  -n, --count <N>      number of samples (default 10)\n"
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
    std::string device = "/dev/spidev0.0";
    int         count  = 10;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) { std::cerr << name << " requires an argument\n"; std::exit(2); }
            return argv[++i];
        };
        if (a == "-d" || a == "--device")      device = next("--device");
        else if (a == "-n" || a == "--count")  count  = parse_int(next("--count"), "--count");
        else if (a == "-h" || a == "--help")  { usage(argv[0]); return 0; }
        else { std::cerr << "unknown option: " << a << "\n"; usage(argv[0]); return 2; }
    }

    embedded::Adxl345 dev(device);
    if (!dev.open()) {
        std::cerr << "failed to open ADXL345 on " << device << "\n";
        return 1;
    }

    std::cout << "sample,x_g,y_g,z_g\n";   // CSV ヘッダ
    for (int i = 0; i < count; ++i) {
        if (auto a = dev.read_g()) {
            std::cout << i << ',' << a->x << ',' << a->y << ',' << a->z << '\n';
        } else {
            std::cerr << "read failed\n";
        }
        usleep(100 * 1000);   // 10Hz でサンプリング
    }

    dev.close();
    return 0;
}
