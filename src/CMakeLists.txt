if(NOT DEFINED SLS_CONFIG_DIR)
    set(SLS_CONFIG_DIR "/etc/sls" CACHE PATH "Default directory for sls.conf")
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core)

add_executable(srt_server ${CMAKE_CURRENT_SOURCE_DIR}/srt-live-server.cpp)
target_link_libraries(srt_server
        sls_core
        spdlog
        -lsrt
        ${CMAKE_THREAD_LIBS_INIT}
)

add_executable(srt_client ${CMAKE_CURRENT_SOURCE_DIR}/srt-live-client.cpp)
target_link_libraries(srt_client
        sls_core
        spdlog
        -lsrt
        ${CMAKE_THREAD_LIBS_INIT}
)

# Copy configuration file
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/sls.conf ${CMAKE_BINARY_DIR}/bin/sls.conf COPYONLY)

# Install binaries
install(TARGETS srt_server srt_client
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install configuration file (only if not already present on the system)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/sls.conf
        DESTINATION ${SLS_CONFIG_DIR}
)