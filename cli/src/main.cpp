#include "../../spi-hal/include/logger.hpp"
#include "../../spi-hal/include/version.hpp"
#include "../../libsensor/include/sensor.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

static void print_version()
{
    std::cout
        << "device-ctl\n"
        << "  cli:       " << embedded::version::CLI       << "\n"
        << "  libsensor: " << embedded::version::LIBSENSOR << "\n"
        << "  spi-hal:   " << embedded::version::SPIHAL    << "\n"
        << "  commit:    " << embedded::version::GIT_COMMIT << "\n"
        << "  built:     " << embedded::version::BUILD_DATE << "\n";
}

static void print_usage(const char* prog)
{
    std::cerr
        << "Usage: " << prog << " [OPTIONS]\n"
        << "\n"
        << "Options:\n"
        << "  -d <device>      SPI デバイスパス (default: /dev/spidev0.0)\n"
        << "  --vref <V>       Vref [V]（default: 3.3）\n"
        << "  --async          read を非同期モードで実行する\n"
        << "  --version        バージョン情報を表示\n"
        << "  -h               このヘルプを表示\n"
        << "\n"
        << "起動後は対話メニューで MCP3008 各チャネルの値を読み出せます。\n";
}

static bool parse_channel(const std::string& s, uint8_t& out)
{
    try {
        int v = std::stoi(s, nullptr, 0);
        if (v < 0 || v >= embedded::Sensor::CHANNEL_COUNT) return false;
        out = static_cast<uint8_t>(v);
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
    auto b = out.find_first_not_of(" \t\r");
    auto e = out.find_last_not_of(" \t\r");
    out = (b == std::string::npos) ? "" : out.substr(b, e - b + 1);
    return true;
}

static void print_reading(uint8_t channel, std::optional<uint16_t> raw, double vref)
{
    if (!raw) {
        std::cerr << "CH" << static_cast<int>(channel) << " read failed\n";
        return;
    }
    double v = static_cast<double>(*raw) * vref / embedded::Sensor::ADC_MAX;
    std::cout << "CH" << static_cast<int>(channel)
              << "  raw=" << *raw
              << "  voltage=" << std::fixed << std::setprecision(3) << v << " V\n";
}

static void do_read_channel(embedded::Sensor& sensor, bool async_mode)
{
    std::string ch_s;
    if (!prompt_line("  チャネル (0-7): ", ch_s)) return;

    uint8_t ch;
    if (!parse_channel(ch_s, ch)) {
        std::cerr << "無効なチャネルです（0〜7）。\n";
        return;
    }

    if (async_mode) {
        std::mutex              mtx;
        std::condition_variable cv;
        bool                    done = false;
        std::optional<uint16_t> result;
        int                     cb_err = 0;

        sensor.read_raw_async(ch, [&](std::optional<uint16_t> raw, int err) {
            result = raw;
            cb_err = err;
            {
                std::lock_guard<std::mutex> lk(mtx);
                done = true;
            }
            cv.notify_one();
        });

        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&]{ return done; });
        if (!result) {
            std::cerr << "async read error: " << cb_err << '\n';
            return;
        }
        print_reading(ch, result, sensor.vref());
    } else {
        print_reading(ch, sensor.read_raw(ch), sensor.vref());
    }
}

static void do_scan_all(embedded::Sensor& sensor)
{
    for (uint8_t ch = 0; ch < embedded::Sensor::CHANNEL_COUNT; ++ch) {
        print_reading(ch, sensor.read_raw(ch), sensor.vref());
    }
}

static void print_menu()
{
    std::cout << "\n[1] チャネル指定読み出し\n"
                 "[2] 全チャネルスキャン\n"
                 "[3] 終了\n";
}

// Background monitor: 1分ごとに CH0 を読み、次のメニュー表示時に表示する。
struct MonitorState {
    std::mutex              mtx;
    std::condition_variable cv;
    std::atomic<bool>       stop{false};
    std::optional<uint16_t> last_raw;
    bool                    pending{false};
};

static void monitor_thread_fn(embedded::Sensor& sensor, MonitorState& state)
{
    while (!state.stop.load()) {
        std::unique_lock<std::mutex> lk(state.mtx);
        state.cv.wait_for(lk, std::chrono::minutes(1),
                          [&]{ return state.stop.load(); });
        if (state.stop.load()) break;
        lk.unlock();

        auto raw = sensor.read_raw(0);

        lk.lock();
        state.last_raw = raw;
        state.pending  = true;
    }
}

static void flush_monitor(MonitorState& state, double vref)
{
    std::lock_guard<std::mutex> lk(state.mtx);
    if (!state.pending) return;
    state.pending = false;
    if (!state.last_raw) {
        std::cout << "[MONITOR] CH0 read failed\n";
    } else {
        double v = static_cast<double>(*state.last_raw) * vref
                   / embedded::Sensor::ADC_MAX;
        std::cout << "[MONITOR] CH0 raw=" << *state.last_raw
                  << " voltage=" << std::fixed << std::setprecision(3)
                  << v << " V\n";
    }
}

static void run_loop(embedded::Sensor& sensor, bool async_mode, MonitorState& state)
{
    while (true) {
        flush_monitor(state, sensor.vref());
        print_menu();

        std::string line;
        if (!prompt_line("選択: ", line)) break;

        if (line == "1") {
            do_read_channel(sensor, async_mode);
        } else if (line == "2") {
            do_scan_all(sensor);
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

    std::string dev_path   = "/dev/spidev0.0";
    double      vref_volts = embedded::Sensor::DEFAULT_VREF;
    bool        async_mode = false;
    int         argi       = 1;

    while (argi < argc && argv[argi][0] == '-') {
        if (std::strcmp(argv[argi], "-d") == 0 && argi + 1 < argc) {
            dev_path = argv[++argi];
        } else if (std::strcmp(argv[argi], "--vref") == 0 && argi + 1 < argc) {
            try {
                vref_volts = std::stod(argv[++argi]);
            } catch (...) {
                std::cerr << "Error: --vref に数値を指定してください\n";
                LOG_CLOSE();
                return EXIT_FAILURE;
            }
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

    embedded::Sensor sensor(dev_path, vref_volts);
    if (!sensor.open()) {
        std::cerr << "Error: cannot open " << dev_path << '\n';
        LOG_CLOSE();
        return EXIT_FAILURE;
    }

    std::cout << "device-ctl 対話モード (デバイス: " << dev_path
              << ", Vref=" << std::fixed << std::setprecision(2)
              << vref_volts << " V)\n";

    MonitorState monitor_state;
    std::thread  monitor_thread(monitor_thread_fn, std::ref(sensor),
                                std::ref(monitor_state));

    run_loop(sensor, async_mode, monitor_state);

    monitor_state.stop.store(true);
    monitor_state.cv.notify_all();
    monitor_thread.join();

    std::cout << "終了します。\n";
    LOG_CLOSE();
    return EXIT_SUCCESS;
}
