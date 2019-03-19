#ifndef EPIC_PING_H
#define EPIC_PING_H

class Ping {
    private:
        // Serialize
        long nonce_;
    public:
        Ping(long nonce) : nonce_(nonce) {}
};

#endif //EPIC_PING_H
