#include <string>
#include <iostream>

#include "init.h"
#include "peer_manager.h"

int main(int argc, char *argv[]) {
    Init(argc, argv);
    PeerManager pm1;
    pm1.Bind("127.0.0.1:7877");
    pm1.Start();

    PeerManager pm2;
    pm2.ConnectTo("127.0.0.1:7877");
    pm2.Start();

    sleep(5);

    pm1.Stop();
    pm2.Stop();
}
