#pragma once

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/epoll.h> 
#include <sys/ioctl.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <fcntl.h>

#include <string>
#include <map>

struct NicePoll {
    std::map<i32, void (*)(i32, u32)> handlers;
    u64 max_events = 64;
    i32 epoll_fd;

    inline i32 create() {
        epoll_fd = epoll_create1(0);
        return epoll_fd;
    }

    inline void insert(i32 fd, u32 events, void (*callback)(i32, u32)) {
        epoll_event event;
        event.events = events;
        event.data.fd = fd;
        handlers[fd] = callback;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    }

    inline void erase(i32 fd) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
        handlers.erase(fd);
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    inline void handle(epoll_event event) {
        i32 fd = event.data.fd;
        handlers[fd](fd, event.events);
    }

    inline i32 wait(epoll_event *events, i32 count, i32 timeout = -1) {
        return epoll_wait(epoll_fd, events, count, timeout);
    }
};

inline i32 make_reusable(i32 fd) {
    const i32 one = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
}

inline i32 make_nonblocking(i32 fd) {
    const i32 one = 1;
    return ioctl(fd, FIONBIO, &one);
}

inline void xsend(i32 fd, std::string x) {
    x += "\n";
    const char *buffer = x.c_str();
    u32 length = x.size();
    u32 written = 0;
    while (written < length)
        written += write(fd, buffer + written, length - written);
}

inline std::string xrecv(i32 fd) {
    u8 prefix[4];
    read(fd, prefix, 4);

    u32 length = 0;
    for (i32 i = 0; i < 4; i++)
        length = length << 8 | prefix[i];

    u8 buffer[1024];
    u32 real_length = 0;
    while (real_length < length)
        real_length += read(fd, buffer, length - real_length);

    std::string s;
    try {
        s = std::string(buffer, buffer + length - 1);
    } catch (...) {
        if (buffer[length-1] != '\n') {
            cout << "[warn] malformed message - " << s << endl;
            u8 sep[1] = {0};
            while (sep[0] != '\n') read(fd, sep, 1);
            cout << "[info] realigned stream\n";
        }
        return "";
    }

    return s;
}
