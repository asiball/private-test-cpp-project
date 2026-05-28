#include "../../spi-hal/include/logger.hpp"
#include "../../spi-hal/include/version.hpp"
#include "../../lib/include/device.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

static void print_version()
{
    std::cout
        << "device-ctl\n"
        << "  cli:     " << embedded::version::CLI    << "\n"
        << "  lib:     " << embedded::version::LIB    << "\n"
        << "  spi-hal: " << embedded::version::SPIHAL << "\n"
        << "  commit:  " << embedded::version::GIT_COMMIT << "\n"
        << "  built:   " << embedded::version::BUILD_DATE << "\n";
}

static void print_usage(const char* prog)
{
    std::cerr
        << "Usage: " << prog << " [OPTIONS]\n"
        << "\n"
        << "Options:\n"
        << "  -d <device>   SPIデバイスパス (default: /dev/spidev0.0)\n"
        << "  --async       非同期モードでreadを実行\n"
        << "  --version     バージョン情報を表示\n"
        << "  -h            このヘルプを表示\n"
        << "\n"
        << "起動後は対話メニューでread/writeを繰り返し実行できます。\n";
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

static bool parse_hex_byte(const std::string& s, uint8_t& out)
{
    try {
        int v = std::stoi(s, nullptr, 0);
        if (v < 0 || v > 255) return false;
        out = static_cast<uint8_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

static bool parse_size(const std::string& s, size_t& out)
{
    try {
        unsigned long v = std::stoul(s);
        if (v == 0) return false;
        out = static_cast<size_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

// Returns false on EOF or stream error.
static bool prompt_line(const std::string& prompt, std::string& out)
{
    std::cout << prompt << std::flush;
    if (!std::getline(std::cin, out)) return false;
    // trim leading/trailing whitespace
    auto b = out.find_first_not_of(" \t\r");
    auto e = out.find_last_not_of(" \t\r");
    out = (b == std::string::npos) ? "" : out.substr(b, e - b + 1);
    return true;
}

static void do_read(embedded::Device& dev, bool async_mode)
{
    std::string reg_s, len_s;
    if (!prompt_line("  レジスタ (hex): ", reg_s)) return;
    if (!prompt_line("  バイト数: ", len_s)) return;

    uint8_t reg;
    size_t  len;
    if (!parse_hex_byte(reg_s, reg)) {
        std::cerr << "無効な入力です。\n";
        return;
    }
    if (!parse_size(len_s, len)) {
        std::cerr << "無効な入力です。\n";
        return;
    }

    if (async_mode) {
        std::mutex              mtx;
        std::condition_variable cv;
        bool done = false;

        dev.read_async(reg, len, [&](const std::vector<uint8_t>& data, int err) {
            if (err) {
                std::cerr << "async read error: " << err << '\n';
            } else {
                print_hex(data);
            }
            {
                std::lock_guard<std::mutex> lk(mtx);
                done = true;
            }
            cv.notify_one();
        });

        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&]{ return done; });
    } else {
        auto data = dev.read(reg, len);
        if (data.empty()) {
            std::cerr << "read failed\n";
            return;
        }
        print_hex(data);
    }
}

static void do_write(embedded::Device& dev)
{
    std::string reg_s, data_s;
    if (!prompt_line("  レジスタ (hex): ", reg_s)) return;
    if (!prompt_line("  データ (hexバイト列、スペース区切り): ", data_s)) return;

    uint8_t reg;
    if (!parse_hex_byte(reg_s, reg)) {
        std::cerr << "無効な入力です。\n";
        return;
    }

    std::vector<uint8_t> data;
    std::istringstream iss(data_s);
    std::string token;
    while (iss >> token) {
        uint8_t b;
        if (!parse_hex_byte(token, b)) {
            std::cerr << "無効な入力です: " << token << "\n";
            return;
        }
        data.push_back(b);
    }
    if (data.empty()) {
        std::cerr << "データが空です。\n";
        return;
    }

    if (!dev.write(reg, data)) {
        std::cerr << "write failed\n";
        return;
    }
    std::cout << "OK\n";
}

static void print_menu()
{
    std::cout << "\n[1] read\n[2] write\n[3] 終了\n";
}

// Background monitor state shared between monitor thread and main loop.
struct MonitorState {
    std::mutex              mtx;
    std::condition_variable cv;
    std::atomic<bool>       stop{false};
    std::vector<uint8_t>    data;
    bool                    pending{false};
    bool                    error{false};
};

static void monitor_thread_fn(embedded::Device& dev, MonitorState& state)
{
    while (!state.stop.load()) {
        std::unique_lock<std::mutex> lk(state.mtx);
        state.cv.wait_for(lk, std::chrono::minutes(1),
                          [&]{ return state.stop.load(); });
        if (state.stop.load()) break;
        lk.unlock();

        auto result = dev.read(0x00, 4);

        lk.lock();
        state.data    = result;
        state.error   = result.empty();
        state.pending = true;
    }
}

static void flush_monitor(MonitorState& state)
{
    std::lock_guard<std::mutex> lk(state.mtx);
    if (!state.pending) return;
    state.pending = false;
    if (state.error) {
        std::cout << "[MONITOR] read failed\n";
    } else {
        std::cout << "[MONITOR] ";
        for (size_t i = 0; i < state.data.size(); ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(state.data[i]);
            if (i + 1 < state.data.size()) std::cout << ' ';
        }
        std::cout << '\n';
    }
}

static void run_loop(embedded::Device& dev, bool async_mode, MonitorState& state)
{
    while (true) {
        flush_monitor(state);
        print_menu();

        std::string line;
        if (!prompt_line("選択: ", line)) break;

        if (line == "1") {
            do_read(dev, async_mode);
        } else if (line == "2") {
            do_write(dev);
        } else if (line == "3") {
            break;
        } else {
            std::cout << "無効な選択です。もう一度入力してください。\n";
        }
    }
}

int main(int argc, char* argv[])
{
    LOG_OPEN("device-ctl");

    std::string dev_path = "/dev/spidev0.0";
    bool async_mode = false;
    int  argi = 1;

    while (argi < argc && argv[argi][0] == '-') {
        if (std::strcmp(argv[argi], "-d") == 0 && argi + 1 < argc) {
            dev_path = argv[++argi];
        } else if (std::strcmp(argv[argi], "--async") == 0) {
            async_mode = true;
        } else if (std::strcmp(argv[argi], "--version") == 0) {
            print_version();
            LOG_CLOSE();
            return EXIT_SUCCESS;
        } else if (std::strcmp(argv[argi], "-h") == 0) {
            print_usage(argv[0]);
            LOG_CLOSE();
            return EXIT_SUCCESS;
        } else {
            std::cerr << "Unknown option: " << argv[argi] << '\n';
            LOG_CLOSE();
            return EXIT_FAILURE;
        }
        ++argi;
    }

    if (argi < argc) {
        std::cerr << "予期しない引数: " << argv[argi] << '\n';
        print_usage(argv[0]);
        LOG_CLOSE();
        return EXIT_FAILURE;
    }

    embedded::Device dev(dev_path);
    if (!dev.open()) {
        std::cerr << "Error: cannot open " << dev_path << '\n';
        LOG_CLOSE();
        return EXIT_FAILURE;
    }

    std::cout << "device-ctl 対話モード (デバイス: " << dev_path << ")\n";

    MonitorState monitor_state;
    std::thread  monitor_thread(monitor_thread_fn, std::ref(dev),
                                std::ref(monitor_state));

    run_loop(dev, async_mode, monitor_state);

    monitor_state.stop.store(true);
    monitor_state.cv.notify_all();
    monitor_thread.join();

    std::cout << "終了します。\n";
    LOG_CLOSE();
    return EXIT_SUCCESS;
}
