project(DobbyTracing)

add_library(${PROJECT_NAME}
        STATIC
        source/PerfettoTracing.cpp
        source/PerfettoTracingSingleton.cpp
)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        include

        PRIVATE
        $<TARGET_PROPERTY:AppInfraLogging,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(${PROJECT_NAME}
        AppInfraTracing
        PerfettoSdk
)