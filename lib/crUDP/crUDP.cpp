
//#include <stdio.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "freertos/event_groups.h"
#include "esp_system.h"
//#include "esp32/rom/ets_sys.h"
//#include "nvs_flash.h"
//#include "driver/gpio.h"
//#include "sdkconfig.h"
//#include "esp_wifi.h"
//#include "esp_event.h"
//#include "esp_netif.h"

#define LOG_LOCAL_LEVEL ESP_LOG_WARN
#include "esp_log.h"

//#include "lwip/err.h"
#include "lwip/sockets.h"
//#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "crUDP.h"

namespace CoReef {

#define MULTICAST_TTL 1
#define LISTEN_ALL_IF true

static const char *MTAG = "crUDP";

MulticastSocket::MulticastSocket (char *multicast_addr, int multicast_port) {
    sock = create_multicast_ipv4_socket(multicast_addr,multicast_port);
    if (sock == -1) return;
    bool success = add_socket_to_multicast_group(sock,multicast_addr,true);
    if (!success) {
        close(sock);
        sock = -1;
    }
    success = create_multicast_ipv4_destination_address(multicast_addr,multicast_port);
    if (!success) {
        close(sock);
        sock = -1;
    }
}

MulticastSocket::~MulticastSocket () {
    if (sock == -1) return;
    // TODO: Leave multicast group
    freeaddrinfo(multicast_destination_address);
    close(sock);
}

bool MulticastSocket::valid () {
    return sock != -1;
}

bool MulticastSocket::add_socket_to_multicast_group (int s, char *multicast_addr, bool assign_source_if) {
    struct ip_mreq imreq = { 0 };
    struct in_addr iaddr = { 0 };
    int err = 0;
    imreq.imr_interface.s_addr = IPADDR_ANY;
    // Configure multicast address to listen to
    err = inet_aton(multicast_addr, &imreq.imr_multiaddr.s_addr);
    if (err != 1) {
        ESP_LOGE(MTAG, "Configured IPV4 multicast address '%s' is invalid.",multicast_addr);
        return false;
    }
    ESP_LOGI(MTAG, "Configured IPV4 Multicast address %s", inet_ntoa(imreq.imr_multiaddr.s_addr));
    if (!IP_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
        ESP_LOGW(MTAG, "Configured IPV4 multicast address '%s' is not a valid multicast address. This will probably not work.",multicast_addr);
    }

    if (assign_source_if) {
        // Assign the IPv4 multicast source interface, via its IP
        // (only necessary if this socket is IPV4 only)
        err = setsockopt(s,IPPROTO_IP,IP_MULTICAST_IF, &iaddr,sizeof(struct in_addr));
        if (err < 0) {
            ESP_LOGE(MTAG, "Failed to set IP_MULTICAST_IF. Error %d", errno);
            return false;
        }
    }

    err = setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq, sizeof(struct ip_mreq));
    if (err < 0) {
        ESP_LOGE(MTAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
        return false;
    }
    return true;
}

int MulticastSocket::create_multicast_ipv4_socket (char *multicast_addr, int multicast_port) {
    struct sockaddr_in saddr = { 0 };
    int s = -1;
    int err = 0;

    s = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s < 0) {
        ESP_LOGE(MTAG, "Failed to create socket. Error %d", errno);
        return -1;
    }

    // Bind the socket to any address
    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(multicast_port);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    err = bind(s, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (err < 0) {
        ESP_LOGE(MTAG, "Failed to bind socket. Error %d", errno);
        close(s);
        return -1;
    }
    
    // Assign multicast TTL (set separately from normal interface TTL)
    uint8_t ttl = MULTICAST_TTL;
    setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(MTAG, "Failed to set IP_MULTICAST_TTL. Error %d", errno);
        close(s);
        return -1;
    }
    return s;
}

bool MulticastSocket::create_multicast_ipv4_destination_address (char *multicast_addr, int multicast_port) {
    struct sockaddr_in sdestv4 = {
        .sin_family = PF_INET,
        .sin_port = htons(multicast_port),
    };
    inet_aton(multicast_addr, &sdestv4.sin_addr.s_addr);

    struct addrinfo hints = {
        .ai_flags = AI_PASSIVE,
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
    };
    int err = getaddrinfo(multicast_addr,NULL,&hints,&multicast_destination_address);
    if (err < 0) {
        ESP_LOGE(MTAG, "getaddrinfo() failed for IPV4 destination address. error: %d", err);
        return false;
        }
    if (multicast_destination_address == 0) {
        ESP_LOGE(MTAG, "getaddrinfo() did not return any addresses");
        return false;
        }
    ((struct sockaddr_in *)multicast_destination_address->ai_addr)->sin_port = htons(multicast_port);
    // char destination_address[32] = { 0 };
    // inet_ntoa_r(((struct sockaddr_in *)res->ai_addr)->sin_addr, destination_address, sizeof(destination_address)-1);
    // ESP_LOGI(MTAG, "Sending to IPV4 multicast address %s:%d...",  destination_address, multicast_port);
    return true;
}

bool MulticastSocket::send (const char *message, int message_len ) {
    if (!valid()) {
        ESP_LOGE(MTAG, "Multicast socket not initialized correctly; unable to send");
        return false;
    }
    int err = sendto(sock, message, message_len, 0, multicast_destination_address->ai_addr, multicast_destination_address->ai_addrlen);
    if (err < 0) {
        ESP_LOGE(MTAG, "IPV4 sendto failed. errno: %d", errno);
        return false;
        }
    return true;
    }
}
