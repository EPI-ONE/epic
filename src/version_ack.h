#ifndef EPIC_VERSION_ACK_H
#define EPIC_VERSION_ACK_H

class VersionACK {
private:
    // now nothing to serialize but it may changed after the P2P protocol is decided

public:
    VersionACK() = default;
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
    }
};

#endif // EPIC_VERSION_ACK_H
