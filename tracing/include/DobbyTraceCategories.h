/*
 * File:   DobbyTraceCategories.h
 *
 * Copyright (C) Sky.uk 2020+
 */

#ifndef DOBBYTRACECATEGORIES_H
#define DOBBYTRACECATEGORIES_H

#if (AI_ENABLE_TRACING)

#include <perfetto.h>

// the set of track event categories that the example is using.
PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("Dobby")
        .SetDescription("Events from code within the Dobby Daemon"),
    perfetto::Category("Plugins")
        .SetDescription("Events from the plugin code"),
    perfetto::Category("NatNetwork")
        .SetDescription("Events from NAT network setup code"),
    perfetto::Category("Containers")
        .SetDescription("Events and counters from running containers"),

);

#endif // AI_ENABLE_TRACING

#endif // DOBBYTRACECATEGORIES_H
