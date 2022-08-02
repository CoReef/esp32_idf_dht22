#pragma once

namespace CoReef {
class MulticastSocket {
    public:
        MulticastSocket (char *multicast_addr, int multicast_port);
        ~MulticastSocket ();

        bool valid ();
    protected:
    private:
        int create_multicast_ipv4_socket(char *multicast_addr, int multicast_port);
        bool add_socket_to_multicast_group (int sock, char *multicast_addr, bool assign_source_if);

        int sock;
};

}