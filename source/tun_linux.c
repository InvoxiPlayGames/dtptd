/*
    tun_linux.c - virtual network interface code for Linux's TUN driver
*/

#ifdef __linux__

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>

#define TUNNEL_IFNAME "dtpt"
// identical IP to the Zune desktop app
// TODO(Emma): make this configurable, make guest IPs increment with new devices
#define TUNNEL_HOSTIP "192.168.55.100"
#define TUNNEL_GUESTIP "192.168.55.101"
#define TUNNEL_NETMASK "255.255.255.0"

static int tun_fd = -1;

int tun_start_interface()
{
    // open the tunnel device
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        perror("couldn't open /dev/net/tun");
        return -1;
    }
    // create a new interface
    struct ifreq ifr = {0};
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, TUNNEL_IFNAME, IFNAMSIZ);
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        perror("couldn't set up TUN interface");
        close(fd);
        return -1;
    }
    // the next need to be done on a socket so make a spare one
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    // set up the IP address for the interface
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, TUNNEL_HOSTIP, &addr.sin_addr);
    memcpy(&ifr.ifr_addr, &addr, sizeof(struct sockaddr));
    if (ioctl(sock, SIOCSIFADDR, &ifr) < 0) {
        perror("couldn't set IP address");
        close(sock);
        close(fd);
        return -1;
    }
    inet_pton(AF_INET, TUNNEL_NETMASK, &addr.sin_addr);
    memcpy(&ifr.ifr_addr, &addr, sizeof(struct sockaddr));
    if (ioctl(sock, SIOCSIFNETMASK, &ifr) < 0) {
        perror("couldn't set subnet mask");
        close(sock);
        close(fd);
        return -1;
    }
    // set the interface up
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        perror("couldn't get interface flags");
        close(sock);
        close(fd);
        return -1;
    }
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if (ioctl(sock, SIOCSIFFLAGS, (void *)&ifr) < 0) {
        perror("couldn't mark interface as up");
        close(sock);
        close(fd);
        return -1;
    }
    close(sock);
    tun_fd = fd;
    return 0;
}

const char *tun_get_ifname()
{
    return TUNNEL_IFNAME;
}

uint32_t tun_get_host_ip()
{
    uint32_t ipv4_addr = 0;
    inet_pton(AF_INET, TUNNEL_HOSTIP, &ipv4_addr);
    return ipv4_addr;
}

uint32_t tun_get_guest_ip()
{
    uint32_t ipv4_addr = 0;
    inet_pton(AF_INET, TUNNEL_GUESTIP, &ipv4_addr);
    return ipv4_addr;
}

int tun_close_interface()
{
    // this should destroy the interface completely, but sometimes it doesn't
    // TODO(Emma): can i make it properly destroy the socket
    close(tun_fd);
    return 0;
}

int tun_dispatch_packet(const uint8_t *packet, size_t packet_sz)
{
    return write(tun_fd, packet, packet_sz);
}

int tun_recv_packet(uint8_t *packet, size_t buf_sz)
{
    return read(tun_fd, packet, buf_sz);
}

#endif // __linux__
