#ifndef EPIC_PONG_H
#define EPIC_PONG_H

class Pong {
    private:
        //Serialize
        long nonce_;
    public:
        Pong(long nonce) : nonce_(nonce) {}
};

#endif //EPIC_PONG_H
