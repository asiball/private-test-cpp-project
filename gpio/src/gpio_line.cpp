#include "gpio_line.hpp"
#include "logger.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <linux/gpio.h>
#include <cerrno>
#include <cstring>

namespace embedded {

GpioLine::GpioLine(const std::string& chip_path, unsigned int offset)
    : chip_path_(chip_path), offset_(offset), chip_fd_(-1), line_fd_(-1), last_errno_(0)
{}

GpioLine::~GpioLine()
{
    close();
}

bool GpioLine::request_edge_events(Edge edge) noexcept
{
    if (line_fd_ >= 0) {
        LOGE("GpioLine::request_edge_events called while already requested: %s offset=%u",
             chip_path_.c_str(), offset_);
        return false;
    }

    chip_fd_ = ::open(chip_path_.c_str(), O_RDONLY | O_CLOEXEC);
    if (chip_fd_ < 0) {
        last_errno_ = errno;
        LOGE("GpioLine::open chip failed: %s (%s)", chip_path_.c_str(), strerror(last_errno_));
        return false;
    }

    struct gpio_v2_line_request req;
    std::memset(&req, 0, sizeof(req));
    req.num_lines  = 1;
    req.offsets[0] = offset_;
    std::strncpy(req.consumer, "embedded-gpio", sizeof(req.consumer) - 1);

    uint64_t edge_flags = 0;
    switch (edge) {
        case Edge::Rising:  edge_flags = GPIO_V2_LINE_FLAG_EDGE_RISING; break;
        case Edge::Falling: edge_flags = GPIO_V2_LINE_FLAG_EDGE_FALLING; break;
        case Edge::Both:    edge_flags = GPIO_V2_LINE_FLAG_EDGE_RISING |
                                         GPIO_V2_LINE_FLAG_EDGE_FALLING; break;
    }
    req.config.flags = GPIO_V2_LINE_FLAG_INPUT | edge_flags;

    if (ioctl(chip_fd_, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
        last_errno_ = errno;
        LOGE("GpioLine::GPIO_V2_GET_LINE_IOCTL failed: offset=%u (%s)",
             offset_, strerror(last_errno_));
        ::close(chip_fd_);
        chip_fd_ = -1;
        return false;
    }

    line_fd_ = req.fd;
    LOGI("GpioLine::request ok: %s offset=%u edge=%d", chip_path_.c_str(), offset_,
         static_cast<int>(edge));
    return true;
}

int GpioLine::wait_event(int timeout_ms) noexcept
{
    if (line_fd_ < 0) {
        last_errno_ = EBADF;
        LOGE("GpioLine::wait_event called before request_edge_events");
        return -1;
    }

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        last_errno_ = errno;
        LOGE("GpioLine::epoll_create1 failed: %s", strerror(last_errno_));
        return -1;
    }

    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.events  = EPOLLIN;
    ev.data.fd = line_fd_;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, line_fd_, &ev) < 0) {
        last_errno_ = errno;
        LOGE("GpioLine::epoll_ctl failed: %s", strerror(last_errno_));
        ::close(epfd);
        return -1;
    }

    struct epoll_event out;
    int n = epoll_wait(epfd, &out, 1, timeout_ms);
    ::close(epfd);

    if (n < 0) {
        last_errno_ = errno;
        LOGE("GpioLine::epoll_wait failed: %s", strerror(last_errno_));
        return -1;
    }
    if (n == 0) {
        return 0;  // タイムアウト
    }

    // イベントを 1 件読み出してキューから取り除く
    struct gpio_v2_line_event event;
    ssize_t r = ::read(line_fd_, &event, sizeof(event));
    if (r < 0) {
        last_errno_ = errno;
        LOGE("GpioLine::read event failed: %s", strerror(last_errno_));
        return -1;
    }
    LOGD("GpioLine::event id=%u offset=%u", event.id, event.offset);
    return 1;
}

void GpioLine::close() noexcept
{
    if (line_fd_ >= 0) {
        ::close(line_fd_);
        line_fd_ = -1;
    }
    if (chip_fd_ >= 0) {
        ::close(chip_fd_);
        chip_fd_ = -1;
    }
}

} // namespace embedded
