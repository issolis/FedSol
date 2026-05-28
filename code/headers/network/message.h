#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
struct AuthMessage
{
    int code;
    std::string password;
    std::string content;
};

struct Message{
    int code; 
    std::string content; 
}; 

#endif 