#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cassert>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include <unordered_map>
#include <vector>
#include <utility>
#include <tuple>
#include <algorithm>
#include "duckchat.h"

#define MAX_SERVER_BUF_SIZE 129

using namespace std;

class Server{
    string host_address;
    string port;
    int listner_fd;
    unordered_map<string, vector<string>> channel_to_ipPortList_map;
    unordered_map<string, pair<string, struct sockaddr *>> ipPort_to_usrInfo_map;

    public:
    Server(string host_address, string port):host_address(host_address), port(port) {
        bind_server();
        respond_clients();
    }

    ~Server() {

    }
    
    void display_members(){
        cout << host_address << endl;
        cout << port << endl;
    }

    private:
    void bind_server() {
        struct addrinfo hints, *server_info;
        int ret_val;
        int yes = 1;
        
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE; // use my IP

        if((ret_val = getaddrinfo(NULL, port.c_str(), &hints, &server_info)) != 0) {
            fprintf(stderr, "selectserver: %s\n", gai_strerror(ret_val));
            exit(1);
        }
        // cout << host_address.c_str() << port.c_str() << endl;
        while(server_info != NULL) {
            listner_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
            if (listner_fd < 0) {
                server_info = server_info->ai_next;
                continue;
            }
            setsockopt(listner_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
            if (bind(listner_fd, server_info->ai_addr, server_info->ai_addrlen) < 0) {
                close(listner_fd);
                server_info = server_info->ai_next;
                continue;
            }

            break;
        }

        if (server_info == NULL) {
            fprintf(stderr, "Server failed to bind\n");
            exit(2);
        }
        freeaddrinfo(server_info);
    }

    void respond_clients() {
        int num_bytes;
        char *buf = new char[MAX_SERVER_BUF_SIZE];
        struct sockaddr_storage client_addr;
        socklen_t addr_len = sizeof(struct sockaddr_in);

        while (1) {
            if((num_bytes = recvfrom(listner_fd, buf, MAX_SERVER_BUF_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
                perror("recvfrom");
                exit(1);
            }
            switch (((struct request *)buf)->req_type) {
                case REQ_LOGIN:
                    cout << "REQ_LOGIN" << endl;
                    process_login_req((struct request_login *)buf, num_bytes, (struct sockaddr *)&client_addr);
                    break;
                case REQ_LOGOUT:
                    cout << "REQ_LOGOUT" << endl;
                    process_logout_req(num_bytes, (struct sockaddr *)&client_addr);
                    break;
                case REQ_JOIN:
                    cout << "REQ_JOIN" << endl;
                    process_join_req((struct request_join *)buf, num_bytes, (struct sockaddr *)&client_addr);
                    break;
                case REQ_LEAVE:
                    cout << "REQ_LEAVE" << endl;
                    process_leave_req((struct request_leave *)buf, num_bytes, (struct sockaddr *)&client_addr);
                    break;
                case REQ_SAY:
                    cout << "REQ_SAY" << endl;
                    process_say_req((struct request_say*) buf, num_bytes, (struct sockaddr *)&client_addr);
                    break;
                case REQ_LIST:
                    cout << "REQ_LIST" << endl;
                    process_list_req(num_bytes, (struct sockaddr *)&client_addr);
                    break;
                case REQ_WHO:
                    cout << "REQ_WHO" << endl;
                    process_who_req((struct request_who*)buf, num_bytes, (struct sockaddr *)&client_addr);
                    break;
                case REQ_KEEP_ALIVE:
                    cout << "REQ_KEEP_ALIVE" << endl;
                    process_keepalive_req((struct request_keep_alive*)buf, num_bytes, (struct sockaddr *)&client_addr);
                    break;
                default:
                    cout << "default" << endl;
                    process_error_req((struct sockaddr *)&client_addr, "Invalid Header");
            }
        }
    }

    void process_error_req(struct sockaddr * client_addr, string err) {
        struct text_error *msg = new struct text_error;
        msg->txt_type = TXT_ERROR;
        strcpy(msg->txt_error, err.c_str());
        send_response(client_addr, msg, sizeof(struct text_error));
    }

    void process_login_req(struct request_login *req, int num_bytes, struct sockaddr *client_addr) {
        if (num_bytes != sizeof(struct request_login)) {
            process_error_req(client_addr, "Login Request: Message length mismatch");
        }
        else {
            string key = get_key_from_sockaddr(client_addr);
            struct sockaddr *client_copy = new struct sockaddr;
            memcpy(client_copy, client_addr, sizeof(struct sockaddr));
            ipPort_to_usrInfo_map[key] = make_pair(string(req->req_username), client_copy);
        }
    }

    void process_logout_req(int num_bytes, struct sockaddr *client_addr) {
        string key = get_key_from_sockaddr(client_addr);
        if (ipPort_to_usrInfo_map.find(key) != ipPort_to_usrInfo_map.end()) {
            if (num_bytes != sizeof(struct request_logout)) {
                process_error_req(client_addr, "Logout Request: Message length mismatch");
            }
            else {
                for (auto map_it = channel_to_ipPortList_map.begin(); map_it != channel_to_ipPortList_map.end();) {
                    auto it = std::find((*map_it).second.begin(), (*map_it).second.end(), key);
                    if (it != (*map_it).second.end()) {
                        swap(*it, (*map_it).second.back());
                        (*map_it).second.pop_back();
                    }

                    // (*map_it).second.erase(find((*map_it).second.begin(), (*map_it).second.end(), key));
                    // Delete Channel info if the lone user in the channel logs out
                    if ((*map_it).second.size() == 0) {
                        map_it = channel_to_ipPortList_map.erase(map_it);
                    }
                    else {
                        ++map_it;
                    }
                }
                // Clear userinfo
                ipPort_to_usrInfo_map.erase(key);
            }
        }
    }

    void process_join_req(struct request_join *req, int num_bytes, struct sockaddr *client_addr) {
        string key = get_key_from_sockaddr(client_addr);
        if (ipPort_to_usrInfo_map.find(key) != ipPort_to_usrInfo_map.end()) {
            if (num_bytes != sizeof(struct request_join)) {
                process_error_req(client_addr, "Join Request: Message length mismatch");
            }
            else {
                string channel(req->req_channel);
                
                if (channel_to_ipPortList_map.find(channel) == channel_to_ipPortList_map.end()) {
                    channel_to_ipPortList_map[channel] = vector<string>();
                }
                if (find(channel_to_ipPortList_map[channel].begin(), channel_to_ipPortList_map[channel].end(), key) != channel_to_ipPortList_map[channel].end()) {
                    process_error_req(client_addr, "You have already joined the Channel.");
                }
                else {
                    if (channel_to_ipPortList_map.size() < CHANNELS_MAX) {
                        channel_to_ipPortList_map[channel].push_back(key);
                    }
                    else {
                        process_error_req(client_addr, "Max Channels limit reached.");
                    }
                }
            }
        }
    }
    
    void process_leave_req(struct request_leave *req, int num_bytes, struct sockaddr *client_addr) {
        string key = get_key_from_sockaddr(client_addr);
        if (ipPort_to_usrInfo_map.find(key) != ipPort_to_usrInfo_map.end()) {
            if (num_bytes != sizeof(struct request_leave)) {
                process_error_req(client_addr, "Leave Request: Message length mismatch");
            }
            else {
                if (channel_to_ipPortList_map.find(req->req_channel) == channel_to_ipPortList_map.end()) {
                    process_error_req(client_addr, "Channel does not exist");
                }
                else {
                    auto &key_list = channel_to_ipPortList_map[req->req_channel];
                    auto it = find(key_list.begin(), key_list.end(), key);
                    if (it == key_list.end()) {
                        process_error_req(client_addr, "Leave Request: You are not subscribed to the Channel");
                    }
                    else {
                        swap(*it, key_list.back());
                        key_list.pop_back();
                        // Remove Channel info if the lone user in the channel leaves
                        if (key_list.size() == 0) {
                            channel_to_ipPortList_map.erase(req->req_channel);
                        }
                    }
                }
            }
        }
    }
    
    void process_say_req(struct request_say *req, int num_bytes, struct sockaddr *client_addr) {
        string key = get_key_from_sockaddr(client_addr);
        if (ipPort_to_usrInfo_map.find(key) != ipPort_to_usrInfo_map.end()) {
            if (num_bytes != sizeof(struct request_say)) {
                process_error_req(client_addr, "Say Request: Message length mismatch");
            }
            else {
                string channel(req->req_channel);
                if (channel_to_ipPortList_map.find(channel) == channel_to_ipPortList_map.end()) {
                    process_error_req(client_addr, "Channel does not exist");
                }
                else {
                    struct text_say *msg = new struct text_say;
                    msg->txt_type = TXT_SAY;
                    memcpy(msg->txt_channel, req->req_channel, sizeof(msg->txt_channel));
                    strcpy(msg->txt_username, ipPort_to_usrInfo_map[key].first.c_str());
                    memcpy(msg->txt_text, req->req_text, sizeof(msg->txt_text));
                    for(auto it:channel_to_ipPortList_map[channel]) {
                        cout << ipPort_to_usrInfo_map[it].first << " : " << ipPort_to_usrInfo_map[it].second << endl;
                        send_response(ipPort_to_usrInfo_map[it].second, msg, sizeof(struct text_say));
                    }
                }
            }
        }
    }
    
    void process_list_req(int num_bytes, struct sockaddr *client_addr) {
        string key = get_key_from_sockaddr(client_addr);
        if (ipPort_to_usrInfo_map.find(key) != ipPort_to_usrInfo_map.end()) {
            if (num_bytes != sizeof(struct request_list)) {
                process_error_req(client_addr, "List Request: Message length mismatch");
            }
            else {
                struct text_list *msg = new struct text_list;
                msg->txt_type = TXT_LIST;
                msg->txt_nchannels = channel_to_ipPortList_map.size();
                int i=0;
                for(auto kv:channel_to_ipPortList_map){
                    strcpy(msg->txt_channels[i++].ch_channel, kv.first.c_str());
                }

                send_response(client_addr, msg, sizeof(struct text_list));
            }
        }
    }
    
    void process_who_req(struct request_who *req, int num_bytes, struct sockaddr *client_addr) {
        string key = get_key_from_sockaddr(client_addr);
        if (ipPort_to_usrInfo_map.find(key) != ipPort_to_usrInfo_map.end()) {
            if (num_bytes != sizeof(struct request_who)) {
                process_error_req(client_addr, "Who Request: Message length mismatch");
            }
            else {
                if (channel_to_ipPortList_map.find(req->req_channel) == channel_to_ipPortList_map.end()) {
                    process_error_req(client_addr, "Channel not found");
                }
                else {
                    struct text_who *msg = new struct text_who;
                    msg->txt_type = TXT_WHO;
                    msg->txt_nusernames = channel_to_ipPortList_map[req->req_channel].size();
                    memcpy(msg->txt_channel, req->req_channel, sizeof(msg->txt_channel));
                    int i = 0;
                    for (auto it: channel_to_ipPortList_map[req->req_channel]) {
                        strcpy(msg->txt_users[i++].us_username, ipPort_to_usrInfo_map[it].first.c_str());
                    }

                    send_response(client_addr, msg, sizeof(struct text_who));
                }
            }
        }
    }
    
    void process_keepalive_req(struct request_keep_alive *req, int num_bytes, struct sockaddr *client_addr) {
        string key = get_key_from_sockaddr(client_addr);
        if (ipPort_to_usrInfo_map.find(key) != ipPort_to_usrInfo_map.end()) {
            if (num_bytes != sizeof(struct request_keep_alive)) {
                process_error_req(client_addr, "Keep-alive Request: Message length mismatch");
            }
            else {
                send_response(client_addr, req, sizeof(struct text_who));
            }
        }
    }

    void send_response(struct sockaddr *to, void* msg, int msgLength) {
        int num_bytes;
        if ((num_bytes = sendto(listner_fd, msg, msgLength, 0, to, sizeof(sockaddr_in))) == -1) {
            perror("sendto");
            exit(1);
        }
        else{
            cout << "sent" << endl;
        }
    }

    string get_key_from_sockaddr(struct sockaddr *client) {
        char s[INET6_ADDRSTRLEN];
        inet_ntop(((struct sockaddr_storage*)client)->ss_family, get_in_addr(client), s, sizeof(s));
        string ip = string(s);
        string port = to_string(ntohs(get_in_port(client)));
        return ip + "-" + port;
    }

    in_port_t get_in_port(struct sockaddr *sa) {
        if (sa->sa_family == AF_INET) {
            return (((struct sockaddr_in*)sa)->sin_port);
        }
        return (((struct sockaddr_in6*)sa)->sin6_port);
    }

    void *get_in_addr(struct sockaddr *sa) {
        if (sa->sa_family == AF_INET) {
            return &(((struct sockaddr_in*)sa)->sin_addr);
        }
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }
};

int main(int argc, char const *argv[])
{
    assert(argc == 3);
    string host_address(argv[1]);
    string port(argv[2]);
    
    Server s = Server(host_address, port);
 
    return 0;
}
