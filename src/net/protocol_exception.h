#include <utility>

#ifndef EPIC_PROTOCOL_EXCEPTION_H
#define EPIC_PROTOCOL_EXCEPTION_H
#include <exception>
#include <string>

class ProtocolException : public std::exception {
    public:
        explicit ProtocolException(std::string msg) : msg_(std::move(msg)) {
        }
        std::string ToString() const {
            return msg_;
        }
    private:
        std::string msg_;
};

#endif //EPIC_PROTOCOL_EXCEPTION_H
