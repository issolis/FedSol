#ifndef DATASETSPLITTER_H
#define DATASETSPLITTER_H

#include <string>

class DatasetSplitter
{
public:
    static void split(const std::string& npzPath, int nClients);
};

#endif