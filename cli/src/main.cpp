#include "../../lib/include/device.hpp"

#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

static void print_usage(const char* prog)
{
    std::cerr
        << "Usage: " << prog << " [OPTIONS] COMMAND\n"
        << "\n"
        << "Commands:\n"
        << "  read  <reg> <len>        レジスタをlen バイト読み出す\n"
        << "  write <reg> <byte>...    レジスタへバイト列を書き込む\n"
        << "\n"
        << "Options:\n"
        << "  -d <device>   SPIデバイスパス (default: /dev/spidev0.0)\n"
        << "  --async       非同期モードで read を実行 (v1.1.0)\n"
        << "  -h            このヘルプを表示\n";
}

static void print_hex(const std::vector<uint8_t>& data)
{
    for (size_t i = 0; i < data.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]);
        if (i + 1 < data.size()) std::cout << ' ';
    }
    std::cout << '\n';
}

int main(int argc, char* argv[])
{
    std::string dev_path = "/dev/spidev0.0";
    bool async_mode = false;
    int  argi = 1;

    // オプション解析
    while (argi < argc && argv[argi][0] == '-') {
        if (std::strcmp(argv[argi], "-d") == 0 && argi + 1 < argc) {
            dev_path = argv[++argi];
        } else if (std::strcmp(argv[argi], "--async") == 0) {
            async_mode = true;
        } else if (std::strcmp(argv[argi], "-h") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            std::cerr << "Unknown option: " << argv[argi] << '\n';
            return EXIT_FAILURE;
        }
        ++argi;
    }

    if (argi >= argc) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    embedded::Device dev(dev_path);
    if (!dev.open()) {
        std::cerr << "Error: cannot open " << dev_path << '\n';
        return EXIT_FAILURE;
    }

    const std::string cmd = argv[argi++];

    if (cmd == "read") {
        if (argi + 1 >= argc) { print_usage(argv[0]); return EXIT_FAILURE; }
        uint8_t reg = static_cast<uint8_t>(std::stoi(argv[argi++], nullptr, 0));
        size_t  len = static_cast<size_t>(std::stoul(argv[argi++]));

        if (async_mode) {
            bool done = false;
            dev.read_async(reg, len, [&done](const std::vector<uint8_t>& data, int err) {
                if (err) {
                    std::cerr << "async read error: " << err << '\n';
                } else {
                    print_hex(data);
                }
                done = true;
            });
            // 簡易待機（実用時はcondition_variableを使うこと）
            while (!done) { /* busy-wait for demo */ }
        } else {
            auto data = dev.read(reg, len);
            if (data.empty()) {
                std::cerr << "read failed\n";
                return EXIT_FAILURE;
            }
            print_hex(data);
        }

    } else if (cmd == "write") {
        if (argi >= argc) { print_usage(argv[0]); return EXIT_FAILURE; }
        uint8_t reg = static_cast<uint8_t>(std::stoi(argv[argi++], nullptr, 0));
        std::vector<uint8_t> data;
        while (argi < argc) {
            data.push_back(static_cast<uint8_t>(std::stoi(argv[argi++], nullptr, 0)));
        }
        if (!dev.write(reg, data)) {
            std::cerr << "write failed\n";
            return EXIT_FAILURE;
        }
        std::cout << "OK\n";

    } else {
        std::cerr << "Unknown command: " << cmd << '\n';
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
