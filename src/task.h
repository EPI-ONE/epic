#ifndef __SRC_TASKS_TASK_H_
#define __SRC_TASKS_TASK_H_

class Task {
};

class GetDataTask : public Task {
    public:
        enum Type {
            PENDING,
            LEVELSET,
        };
};

class GetBlockTask : public Task {

};

#endif // ifndef __SRC_TASKS_TASK_H_
