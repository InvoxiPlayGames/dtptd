/*
    tun_stub.c - virtual network interface code for build targets that don't
    have tunnelling code
*/

#if !__linux__ && !__APPLE__

#include <stdint.h>
#include <arpa/inet.h>

int tun_start_interface()
{
    return -1;
}

const char *tun_get_ifname()
{
    return "stub";
}

uint32_t tun_get_host_ip()
{
    uint32_t ipv4_addr = 0;
    inet_pton(AF_INET, "192.168.55.100", &ipv4_addr);
    return ipv4_addr;
}

uint32_t tun_get_guest_ip()
{
    uint32_t ipv4_addr = 0;
    inet_pton(AF_INET, "192.168.55.101", &ipv4_addr);
    return ipv4_addr;
}

int tun_close_interface()
{
    return 0;
}

int tun_dispatch_packet(const uint8_t *packet, size_t packet_sz)
{
    return 0;
}

int tun_recv_packet(uint8_t *packet, size_t buf_sz)
{
    return 0;
}

#endif // __linux__
