
add_executable( ethanlog-cat
        ethanlog-cat.cpp )

add_dependencies( ethanlog-cat
        ethanlog )

target_link_libraries( ethanlog-cat
        PRIVATE
        ethanlog )

target_include_directories( ethanlog-cat
        PRIVATE
        $<TARGET_PROPERTY:ethanlog,INTERFACE_INCLUDE_DIRECTORIES> )


install( TARGETS ethanlog-cat
         RUNTIME DESTINATION bin )
