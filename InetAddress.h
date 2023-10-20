#ifndef INETADDRESS_H
#define INETADDRESS_H

#pragma once
#include <cstdint>
#include <netinet/in.h>
#include <string>

// 封装socket的ip地址及端口号
class InetAddress {
public:
    explicit InetAddress(uint16_t port = 8080, std::string ip = "127.0.0.1"); // 传入端口号和ip地址进行构造
    explicit InetAddress(const sockaddr_in& addr) // 传入sockaddr_in变量进行构造
        : addr_(addr)
    {
    }

    std::string toIp() const; // 获取IP地址
    std::string toIpPort() const; // 获取IP:Port
    uint16_t toPort() const; // 获取端口号

    const sockaddr_in* getSockAddr() const { return &addr_; }
    void setSockAddr(const sockaddr_in& addr) { addr_ = addr; }

private:
    struct sockaddr_in addr_; // IPV4地址，C++中可以省略struct，因为struct在C++中视为一个类
};

#endif