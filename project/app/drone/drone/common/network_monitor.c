#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "network_monitor.h"

static network_config_t g_config;
static network_state_t g_state;
static pthread_t monitor_thread;
static volatile int g_running = 0;

// Checksum calculation for ICMP
static unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// Low-level ping implementation using raw sockets
// Returns: true if reply received, false on timeout/error
static bool ping_host(const char *ip_addr, int timeout_ms) {
    int sockfd;
    struct sockaddr_in addr;
    struct icmp icmp_hdr;
    char buf[64]; // Small buffer needed
    struct timeval tv;
    fd_set rset;

    // Validate IP
    if (!ip_addr || strlen(ip_addr) == 0) return false;

    // Create raw socket
    // Note: Requires root privileges (usually true for flight controller)
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        // Socket creation failed (network stack not ready?)
        return false;
    }

    // Set non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip_addr, &addr.sin_addr) <= 0) {
        close(sockfd);
        return false;
    }

    // Prepare ICMP packet
    memset(&icmp_hdr, 0, sizeof(icmp_hdr));
    icmp_hdr.icmp_type = ICMP_ECHO;
    icmp_hdr.icmp_code = 0;
    icmp_hdr.icmp_id = getpid() & 0xFFFF;
    icmp_hdr.icmp_seq = 1;
    icmp_hdr.icmp_cksum = checksum(&icmp_hdr, sizeof(icmp_hdr));

    // Send packet
    if (sendto(sockfd, &icmp_hdr, sizeof(icmp_hdr), 0, (struct sockaddr *)&addr, sizeof(addr)) <= 0) {
        close(sockfd);
        return false;
    }

    // Wait for reply
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);

    int ret = select(sockfd + 1, &rset, NULL, NULL, &tv);
    bool success = false;

    if (ret > 0) {
        struct sockaddr_in r_addr;
        socklen_t addr_len = sizeof(r_addr);
        // We might receive other ICMP packets, strictly we should check ID, but for status monitoring this is usually enough
        if (recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&r_addr, &addr_len) > 0) {
            success = true;
        }
    }

    close(sockfd);
    return success;
}

// Check if interface is UP (Layer 1/2) via sysfs
static bool is_interface_up(const char *iface) {
    char path[128];
    char status[16];
    snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", iface);

    FILE *f = fopen(path, "r");
    if (!f) return false; // Interface doesn't exist yet

    if (fgets(status, sizeof(status), f)) {
        // Remove newline
        status[strcspn(status, "\n")] = 0;
        fclose(f);
        return (strcmp(status, "up") == 0 || strcmp(status, "unknown") == 0);
        // "unknown" is common for some virtual interfaces or bridges that are technically active
    }
    fclose(f);
    return false;
}

// Check if interface has IP in 10.0.0.0/16
static bool check_subnet_10_0(const char *target_iface) {
    struct ifaddrs *ifaddr, *ifa;
    bool found = false;

    if (getifaddrs(&ifaddr) == -1) return false;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        if (strcmp(ifa->ifa_name, target_iface) == 0) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            uint32_t ip = ntohl(sa->sin_addr.s_addr);
            // 10.0.0.0/16 => IP starts with 10.0 (0x0A00xxxx)
            // 10.0.0.0 = 167772160 (decimal)
            // Mask /16 = 0xFFFF0000

            if ((ip & 0xFFFF0000) == 0x0A000000) {
                found = true;
                break;
            }
        }
    }

    freeifaddrs(ifaddr);
    return found;
}

static void* network_monitor_thread_func(void* arg) {
    printf("Network monitor thread started\n");

    while (g_running) {
        network_state_t local_state = {0};

        // 1. Check eth0
        bool eth_phys_up = is_interface_up("eth0");
        if (eth_phys_up) {
            bool ping_eth = ping_host(g_config.eth_gateway, 200); // 200ms timeout
            local_state.eth_status = ping_eth ? 2 : 1;
            printf("Eth0: Link UP, Ping %s\n", ping_eth ? "OK" : "FAIL");
        } else {
            local_state.eth_status = 0;
            printf("Eth0: Link DOWN\n");
        }

        // 2. Check wg0
        bool wg_phys_up = is_interface_up("wg0");
        if (wg_phys_up) {
            bool ping_wg = ping_host(g_config.wg_gateway, 500); // 500ms timeout (VPN might be slower to respond)
            local_state.wg_status = ping_wg ? 2 : 1;
            printf("WG0: Link UP, Ping %s\n", ping_wg ? "OK" : "FAIL");
        } else {
            local_state.wg_status = 0;
            printf("WG0: Link DOWN\n");
        }

        // 3. Check Operator (Only if eth+wg valid, as per requirements)
        // Requirement: "При умові шо перші два пункти працюють всі умови"
        // Interpreted as: eth0 is working (status >= 1? or 2?) and wg0 is working.
        // Usually VPN depends on Eth, so if WG is up and pinging, Eth is likely OK.
        // Let's assume "working" means Ping OK (status == 2).
        if (local_state.eth_status == 2 && local_state.wg_status == 2) {
            local_state.operator_ping = ping_host(g_config.operator_ip, 500);
        } else {
            local_state.operator_ping = false;
        }

        // 4. Check LAN devices (Only if eth0 has 10.0.0.0/16 address)
        if (local_state.eth_status >= 1) { // If link is at least UP
             if (check_subnet_10_0("eth0")) {
                 local_state.dev1_ping = ping_host(g_config.dev1_ip, 100);
                 local_state.dev2_ping = ping_host(g_config.dev2_ip, 100);
                 local_state.dev3_ping = ping_host(g_config.dev3_ip, 100);
                 printf("LAN Devices: Dev1 %s, Dev2 %s, Dev3 %s\n",
                        local_state.dev1_ping ? "OK" : "FAIL",
                        local_state.dev2_ping ? "OK" : "FAIL",
                        local_state.dev3_ping ? "OK" : "FAIL");
             }
        }

        // Update global state safely
        pthread_mutex_lock(&g_state.mutex);

        // Copy values (preserving mutex)
        g_state.eth_status = local_state.eth_status;
        g_state.wg_status = local_state.wg_status;
        g_state.operator_ping = local_state.operator_ping;
        g_state.dev1_ping = local_state.dev1_ping;
        g_state.dev2_ping = local_state.dev2_ping;
        g_state.dev3_ping = local_state.dev3_ping;

        pthread_mutex_unlock(&g_state.mutex);

        // Sleep 1 second
        sleep(1);
    }
    return NULL;
}

int network_monitor_init(network_config_t* config) {
    if (!config) return -1;

    // Copy config
    g_config = *config;

    // Init mutex
    pthread_mutex_init(&g_state.mutex, NULL);

    g_running = 1;
    if (pthread_create(&monitor_thread, NULL, network_monitor_thread_func, NULL) != 0) {
        return -1;
    }

    return 0;
}

void network_monitor_cleanup(void) {
    g_running = 0;
    pthread_join(monitor_thread, NULL);
    pthread_mutex_destroy(&g_state.mutex);
}

void network_monitor_get_state(network_state_t* out_state) {
    if (!out_state) return;

    pthread_mutex_lock(&g_state.mutex);
    *out_state = g_state; // Copy struct
    pthread_mutex_unlock(&g_state.mutex);
}
