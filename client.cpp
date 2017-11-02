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
#include <vector>
#include <algorithm>
#include <time.h>
#include <signal.h>
#include "duckchat.h"
#include "raw.h"

#define MAX_CLIENT_BUF_SIZE 385

using namespace std;

static void handle_ctrl_c(int s) {
    cooked_mode();
    cout << endl;
    exit(s);
}

class Client{
    string host_name;
    string port;
    string username;
    string input;
    int sock_fd;
    struct addrinfo *server_info;
    vector<string> channels_list;
    time_t last_send_msg_time;

    public:
    Client(string host_name, string port, string username):host_name(host_name),port(port),username(username){

        set_sockfd_serverinfo();
        struct sigaction sigIntHandler;

        sigIntHandler.sa_handler = handle_ctrl_c;
        sigemptyset(&sigIntHandler.sa_mask);
        sigIntHandler.sa_flags = 0;

        sigaction(SIGINT, &sigIntHandler, NULL);

        send_login_req();
        channels_list.push_back("Common");
        send_join_req(channels_list.back());

        raw_mode();
        begin_chat();
    }


    ~Client() {
        cooked_mode();
        freeaddrinfo(server_info);
    }

    void display_members(){
        cout << host_name << endl;
        cout << port << endl;
        cout << username << endl;
    }

    private:
    void set_sockfd_serverinfo() {
        struct addrinfo hints;
        int ret_val;

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
            exit(1);
        }
    }

    void send_request(void* msg, int msgLength) {
        int num_bytes;
        if ((num_bytes = sendto(sock_fd, msg, msgLength, 0, server_info->ai_addr, server_info->ai_addrlen)) == -1) {
            perror("sendto");
            exit(1);
        }
        time(&last_send_msg_time);
    }

    void send_login_req() {
        struct request_login req;
        req.req_type = REQ_LOGIN;
        strcpy(req.req_username, username.c_str());
 
        send_request(&req, sizeof(struct request_login));
    }

    void send_join_req(string channel) {
        struct request_join req;
        req.req_type = REQ_JOIN;
        strcpy(req.req_channel, channel.c_str());

        send_request(&req, sizeof(struct request_join));
    }

    void send_leave_req(string channel) {
        struct request_leave req;
        req.req_type = REQ_LEAVE;
        strcpy(req.req_channel, channel.c_str());

        send_request(&req, sizeof(struct request_leave));
    }

    void send_list_req() {
        struct request_list req;
        req.req_type = REQ_LIST;

        send_request(&req, sizeof(struct request_list));
    }

    void send_who_req(string channel) {
        struct request_who req;
        req.req_type = REQ_WHO;
        strcpy(req.req_channel, channel.c_str());

        send_request(&req, sizeof(struct request_who));
    }

    void send_say_req(string text) {
        struct request_say req;
        req.req_type = REQ_SAY;
        strcpy(req.req_channel, channels_list.back().c_str());
        strcpy(req.req_text, text.c_str());

        send_request(&req, sizeof(struct request_say));
    }

    void send_logout_req() {
        struct request_logout req;
        req.req_type = REQ_LOGOUT;

        send_request(&req, sizeof(struct request_logout));
    }

    void send_keepalive_req() {
        struct request_keep_alive req;
        req.req_type = REQ_KEEP_ALIVE;

        send_request(&req, sizeof(struct request_keep_alive));
    }

    void begin_chat() {
        fd_set read_fds;
        fd_set master_read_fds;
        int fdmax;
                
        char in;
        int num_bytes;
        struct sockaddr_storage client_addr;
        socklen_t addr_len = sizeof(struct sockaddr_in);
        char buf[MAX_CLIENT_BUF_SIZE];
        struct timeval tv;
        time_t now;
        double seconds;

        tv.tv_sec = 30;
        tv.tv_usec = 0;

        FD_ZERO(&read_fds);
        FD_ZERO(&master_read_fds);

        FD_SET(0, &master_read_fds);
        FD_SET(sock_fd, &master_read_fds);

        fdmax = sock_fd;
        cout << "> ";
        fflush(0);
        while (1) {
            read_fds = master_read_fds;

            if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1) {
                perror("select");
                exit(4);
            }
            if (FD_ISSET(0, &read_fds)) {
                in = getchar();
                if (in == 10) {
                    if(process_user_input())
                        break;
                }
                else {
                    if (input.length() < SAY_MAX) {
                        input += in;
                        putchar(in);
                        fflush(0);
                    }
                }
            }
            if(FD_ISSET(sock_fd, &read_fds)) {
                if((num_bytes = recvfrom(sock_fd, buf, MAX_CLIENT_BUF_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
                    perror("recvfrom");
                    exit(1);
                }
                switch (((struct text *)buf)->txt_type) {
                    case TXT_SAY: process_say_msg((struct text_say *)buf);
                                break;
                    case TXT_LIST: process_list_msg((struct text_list *)buf);
                                break;
                    case TXT_WHO: process_who_msg((struct text_who *)buf);
                                break;
                    case TXT_ERROR: process_error_msg((struct text_error *)buf);
                                break;
                    default: break;
                }
            }
            time(&now);
            seconds = difftime(now, last_send_msg_time);
            if(seconds > 59) {
                send_keepalive_req();
            }
        }
    }

    int process_user_input() {
        if (input.find("/join ") == 0) {
            string channel = input.substr(6);
            if (channel.length() <= CHANNEL_MAX) {
                if (find(channels_list.begin(), channels_list.end(), channel) == channels_list.end()) {
                    channels_list.push_back(channel);
                    send_join_req(channel);
                }
                else{
                    cout << "You have already subscribed to the channel: " << channel;
                }
            }
            else
                cout << "Channel name length limit exceeded";
        }
        else if (input.find("/leave ") == 0) {
            string channel = input.substr(7);
            auto it = find(channels_list.begin(), channels_list.end(), channel);
            if (it == channels_list.end()) {
                cout << "Error: Channel \'" << channel << "\' not found\n";
            }
            else {
                channels_list.erase(it);
                send_leave_req(channel);
            }
        }
        else if (input.find("/switch ") == 0) {
            string channel = input.substr(8);
            auto it = find(channels_list.begin(), channels_list.end(), channel);
            if (it == channels_list.end()) {
                cout << "Error: Channel \'" << channel << "\' not found\n";
            }
            else {
                channels_list.erase(it);
                channels_list.push_back(channel);
            }
        }
        else if (input == "/list") {
            send_list_req();
        }
        else if (input.find("/who ") == 0) {
            string channel = input.substr(5);
            if (channel.length() <= CHANNEL_MAX) {
                send_who_req(channel);
            }
            else {
                cout << "Error: Channel name length limit exceeded\n";
            }
        }
        else if (input == "/exit") {
            send_logout_req();
            cout << endl;
            return 1;
        }
        else if (input[0] == '/') {
            cout << "Invalid command" << endl;
        }
        else if (input.length() <= SAY_MAX){
            send_say_req(input);
        }
        input = "";
        cout << "\n";
        cout << "> ";
        return 0;
    }

    void process_say_msg(struct text_say *msg) {
        clear_buffer();
        cout << "[" << string(msg->txt_channel) << "] [" << string(msg->txt_username) << "]";
        cout << ": " << string(msg->txt_text) << "\n";
        reinsert_buffer();
    }

    void process_list_msg(struct text_list *msg) {
        clear_buffer();
        cout << "Existing channels:" << "\n";
        for (int i = 0; i < msg->txt_nchannels; ++i) {
            cout << string(msg->txt_channels[i].ch_channel) << "\n";
        }
        reinsert_buffer();
    }

    void process_who_msg(struct text_who *msg) {
        clear_buffer();
        cout << "Users on channel" << string(msg->txt_channel) << ":" << "\n";
        for (int i = 0; i < msg->txt_nusernames; ++i) {
            cout << string(msg->txt_users[i].us_username) << "\n";
        }
        reinsert_buffer();
    }

    void process_error_msg(struct text_error *msg) {
        clear_buffer();
        cout << "Error: " << string(msg->txt_error) << "\n";
        reinsert_buffer();
    }

    void clear_buffer() {
        for (unsigned int i = 0; i < input.length()+2; ++i) {
            cout << "\b \b";
        }        
    }

    void reinsert_buffer() {
        cout << "> " << input;
        fflush(0);
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
    //c.display_members();
    
    return 0;
}
