#include "caterpillar.h"
#include "dag_manager.h"
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

        auto height = CAT->GetHeadHeight();
        for (int i = 0; i < height; i++) {
            auto l = CAT->GetMilestoneAt(i);
            std::cout << l->height << ": " << l->cblock->GetHash().to_substr() << std::endl;
        }

        WaitShutdown();
    } else {
        std::cout << "Failed to start epic" << std::endl;
    }
    ShutDown();
    return NORMAL_EXIT;
}
