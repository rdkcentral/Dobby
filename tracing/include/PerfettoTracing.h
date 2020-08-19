#ifndef PERFETTOTRACING_H
#define PERFETTOTRACING_H

#include <perfetto.h>
#include <string>


class PerfettoTracing
{
public:
    enum Backend : unsigned
    {
        SystemBackend = 0x1,
        InProcessBackend = 0x2,
    };

    static bool initialise(unsigned backends = (SystemBackend | InProcessBackend));

    static bool isTracing();

    static bool startInProcessTracing(const std::string &traceFile,
                                      const std::string &categoryFilter = std::string());
    static bool startInProcessTracing(int fd,
                                      const std::string &categoryFilter = std::string());
    static void stopInProcessTracing();
};

#endif