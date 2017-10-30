#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cassert>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include "duckchat.h"
#include "raw.h"

using namespace std;

class Client{
    string host_name;
    string port;
    string username;
    int sock_fd;
    struct addrinfo *server_info;    

    public:
    Client(string host_name, string port, string username):host_name(host_name),port(port),username(username){
        send_login_req();
    }

    void send_login_req() {
        struct addrinfo hints;
        int ret_val;
        int num_bytes;
        void *req = new struct request_login;
        ((struct request_login*) req)->req_type = REQ_LOGIN;
        strcpy(((struct request_login*)req)->req_username, username.c_str());
 
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        
        if ((ret_val = getaddrinfo(host_name.c_str(), port.c_str(), &hints, &server_info)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret_val));
            return;
        }
        while(server_info != NULL) {
            if ((sock_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol)) == -1) {
                perror("talker: socket");
                server_info = server_info->ai_next;
                continue;
            }
            break;
        }
        if (server_info == NULL) {
            fprintf(stderr, "Failed to create socket\n");
            return;
        }
        if ((num_bytes = sendto(sock_fd, req, sizeof(struct request_login), 0, server_info->ai_addr, server_info->ai_addrlen)) == -1) {
            perror("send_login_req - sendto");
            exit(1);
        }
    }

    ~Client() {
        freeaddrinfo(server_info);
    }

    void display_members(){
        cout << host_name << endl;
        cout << port << endl;
        cout << username << endl;
    }
};

int main(int argc, char const *argv[])
{
    assert(argc == 4);

    string host_name(argv[1]);
    string port(argv[2]);
    string username(argv[3]);
    
    assert(host_name.length()<=UNIX_PATH_MAX);

    Client c = Client(host_name, port, username);
    c.display_members();
    
    return 0;
}
