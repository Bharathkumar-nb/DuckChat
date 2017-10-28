#include <iostream>
#include <cassert>
#include <string>
#include "duckchat.h"
#include "raw.h"

using namespace std;

int main(int argc, char const *argv[])
{
    assert(argc == 4);    

    string host_name(argv[1]);
    string port(argv[2]);
    string username(argv[3]);

    cout << host_name << endl;
    cout << port << endl;
    cout << username << endl;	

    return 0;
}
