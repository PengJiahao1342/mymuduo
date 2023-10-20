#include "InetAddress.h"
#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <strings.h>
#include <sys/socket.h>

// 传入端口号和ip地址进行构造
InetAddress::InetAddress(uint16_t port, std::string ip)
{
    // 清零后对addr_进行赋值
    bzero(&addr_, sizeof(addr_));
    addr_.sin_family = AF_INET; // IPV4
    addr_.sin_port = htons(port); // 主机字节序转换为网络字节序
    addr_.sin_addr.s_addr = inet_addr(ip.c_str()); // 点分十进制转换为网络字节序的整数
}

// 获取IP地址
std::string InetAddress::toIp() const
{
    // addr_赋值后都是网络字节序，需要转换为主机字节序
    char buf[64] = { 0 };
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    return buf;
}

// 获取IP:Port
std::string InetAddress::toIpPort() const
{
    char buf[64] = { 0 };
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);

    size_t ipEnd = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf + ipEnd, ":%u", port);

    return buf;
}

// 获取端口号
uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}


// test
// #include <iostream>
// int main()
// {
//     InetAddress testIp(8080, "192.168.110.4");
//     std::cout << testIp.toIpPort() << std::endl;

//     return 0;
// }