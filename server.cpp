#include <iostream>
#include <cassert>
#include <string>
#include "duckchat.h"
#include "raw.h"

using namespace std;

int main(int argc, char const *argv[])
{
    assert(argc == 3);
    string host_address(argv[1]);
    string port(argv[2]);

    cout << host_address << endl;
    cout << port << endl;
    return 0;
}
