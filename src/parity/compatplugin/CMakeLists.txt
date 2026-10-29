# A widget-interface plugin for the parity tests; kept as a binary from phase 0 and loaded
# unchanged later to prove old plugins still work. Built into the build tree only, never installed.
add_library(konsole_compatplugin MODULE compatplugin.cpp)
configure_file(compatplugin.in.json compatplugin.json)
target_link_libraries(konsole_compatplugin konsoleprivate konsoleapp KF6::CoreAddons)
set_target_properties(konsole_compatplugin PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/parity-plugins/konsoleplugins")
