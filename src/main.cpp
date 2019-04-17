#include <string>
#include <iostream>

#include "init.h"
#include "peer_manager.h"
int main(int argc, char *argv[]) {
    Init(argc, argv);
    PeerManager pm;
    pm.Start();
    pm.Stop();
}
