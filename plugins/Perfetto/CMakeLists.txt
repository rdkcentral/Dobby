
add_library( PerfettoPlugin
        MODULE
        source/PerfettoPlugin.cpp
        $<TARGET_OBJECTS:DobbyPluginCommonLib>
        )

target_include_directories( PerfettoPlugin
        PRIVATE
        $<TARGET_PROPERTY:DobbyPluginCommonLib,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:DobbyDaemonLib,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:AppInfraLogging,INTERFACE_INCLUDE_DIRECTORIES>
        )

set_target_properties( PerfettoPlugin PROPERTIES
        VERSION   ${DOBBY_MAJOR_VERSION}.1.0
        SOVERSION ${DOBBY_MAJOR_VERSION}
        )

install(
        TARGETS PerfettoPlugin
        LIBRARY DESTINATION lib/plugins/dobby
)
