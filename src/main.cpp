#include "init.h"

int main(int argc, char** argv) {
    int ret = 0;
    Init(argc, argv);
    if (Start()) {
        WaitShutdown();
    } else {
        std::cout << "Failed to start epic" << std::endl;
        ret = -1;
    }
    ShutDown();
    return ret;
}
