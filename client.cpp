#include <iostream>
#include <cassert>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include "duckchat.h"
#include "raw.h"

using namespace std;

class Client{
    string host_name;
    string port;
    string username;
    
    public:
    Client(string host_name, string port, string username):host_name(host_name),port(port),username(username){
        
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
        

    return 0;
}
