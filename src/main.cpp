#include <iostream>
#include "sstream"
#include <string>
#include "net/peer_manager.h"
#include "net/net_address.h"
using namespace std;
int main(){
    std::string ip_s = "192.168.0.1:7877";
    NetAddress ip(ip_s);
    cout<<ip.ToString();
}

