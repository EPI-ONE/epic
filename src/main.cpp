#include <iostream>
#include <string>

#include "address_manager.h"
#include "cpptoml.h"
#include "init.h"
#include "peer_manager.h"

int main(int argc, char* argv[]) {
    Init(argc, argv);
    AddressManager addressManager;
    addressManager.Init();
    return 0;
}
