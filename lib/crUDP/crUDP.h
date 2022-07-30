#pragma once

class crUDP {
    public:
        crUDP ();
        ~crUDP ();

        int create_multicast_ipv4_socket(char *multicast_addr, int multicast_port);
    protected:
    private:
        bool add_socket_to_multicast_group (int sock, char *multicast_addr, bool assign_source_if);
};