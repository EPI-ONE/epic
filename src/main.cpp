#include "init.h"

int main(int argc, char** argv) {
    int init_result = Init(argc, argv);
    if (!init_result) {
        CreateDaemon();
    } else {
        return init_result;
    }

    if (Start()) {
        // start some applications, such as miner, visualization...
        // TODO
        WaitShutdown();
    } else {
        std::cout << "Failed to start epic" << std::endl;
    }
    ShutDown();
    return NORMAL_EXIT;
}
