#ifndef SOCKET_HANDLE_HPP
#define SOCKET_HANDLE_HPP

#include <unistd.h>

class SocketHandle {
public:
    explicit SocketHandle(int fd = -1)
        : fd_(fd) {
    }

    ~SocketHandle() {
        close_if_valid();
    }

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    SocketHandle(SocketHandle&& other) noexcept
        : fd_(other.release()) {
    }

    SocketHandle& operator=(SocketHandle&& other) noexcept {
        if (this != &other) {
            close_if_valid();
            fd_ = other.release();
        }

        return *this;
    }

    int get() const {
        return fd_;
    }

    bool valid() const {
        return fd_ != -1;
    }

    int release() {
        const int released_fd = fd_;
        fd_ = -1;
        return released_fd;
    }

    void reset(int fd = -1) {
        if (fd_ != fd) {
            close_if_valid();
            fd_ = fd;
        }
    }

private:
    void close_if_valid() {
        if (fd_ != -1) {
            close(fd_);
            fd_ = -1;
        }
    }

    int fd_;
};

#endif
