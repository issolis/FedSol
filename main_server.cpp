#include <iostream>

#include "network/server.h"

int main()
{
    unsigned short port = 9000;
    uint32_t backlog = 10;

    Server server(port, backlog);

    std::cout << "Starting server..." << std::endl;

    server.run();

    return 0;
}