/*
    tun_macos.c - virtual network interface code for macOS's utun interface
*/

#ifdef __APPLE__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_utun.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_var.h>
#include <sys/_endian.h>
#include <sys/_types/_socklen_t.h>

// identical IP to the Zune desktop app
// TODO(Emma): make this configurable, make guest IPs increment with new devices
#define TUNNEL_ROUTE "192.168.55.0"
#define TUNNEL_HOSTIP "192.168.55.100"
#define TUNNEL_GUESTIP "192.168.55.101"
#define TUNNEL_NETMASK "255.255.255.0"

#define UTUN_CONTROL_NAME "com.apple.net.utun_control"

static char tun_name[IF_NAMESIZE] = {0};
static int tun_fd = -1;

int tun_start_interface()
{
    // open a control socket
    int fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
    if (fd < 0) {
        perror("couldn't open control socket");
        return -1;
    }
    // set it as a utun control socket
    struct ctl_info ci = {0};
    strncpy(ci.ctl_name, UTUN_CONTROL_NAME, sizeof(UTUN_CONTROL_NAME));
    if (ioctl(fd, CTLIOCGINFO, &ci) < 0) {
        perror("couldn't set up utun control socket");
        close(fd);
        return -1;
    }
    // create a new utun connection
    struct sockaddr_ctl sc = {0};
    sc.sc_len = sizeof(struct sockaddr_ctl);
    sc.sc_family = AF_SYSTEM;
    sc.ss_sysaddr = AF_SYS_CONTROL;
    sc.sc_id = ci.ctl_id;
    sc.sc_unit = 0; // auto-assign
    if (connect(fd, (struct sockaddr *)&sc, sizeof(struct sockaddr_ctl)) < 0) {
        perror("couldn't connect to utun socket");
        close(fd);
        return -1;
    }
    // get the interface name
    socklen_t ifname_len = sizeof(tun_name);
    if (getsockopt(fd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, tun_name, &ifname_len) < 0) {
        perror("couldn't connect to utun socket");
        close(fd);
        return -1;
    }

    // the next need to be done on a socket so make a spare one
    struct ifreq ifr = {0};
    strncpy(ifr.ifr_name, tun_name, sizeof(ifr.ifr_name));
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
    // TODO(Emma): configure routing automatically
    // for now, if you run "sudo route -n add -net 192.168.55.0/24 -interface utunX", it will work
    tun_fd = fd;
    return 0;
}

const char *tun_get_ifname()
{
    return tun_name;
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
    // this should destroy the interface completely
    close(tun_fd);
    return 0;
}

int tun_dispatch_packet(const uint8_t *packet, size_t packet_sz)
{
    // TODO(Emma): try to avoid a copy
    uint8_t fullbuf[2052];
    if (packet_sz > (sizeof(fullbuf) - 4))
        return -1;
    *(uint32_t *)fullbuf = htonl(AF_INET);
    memcpy(fullbuf + 4, packet, packet_sz);
    return write(tun_fd, fullbuf, packet_sz + 4);
}

int tun_recv_packet(uint8_t *packet, size_t buf_sz)
{
    // TODO(Emma): try to avoid a copy
    uint8_t fullbuf[2052];
    int r = read(tun_fd, fullbuf, sizeof(fullbuf));
    if (r >= 4) {
        if (*(uint32_t *)fullbuf == htonl(AF_INET)) {
            if ((r - 4) <= buf_sz)
                memcpy(packet, fullbuf + 4, r - 4);
            return r - 4;
        } else {
            return -1; // not IPv4, drop the packet
        }
    }
    return r;
}

#endif // __APPLE__
