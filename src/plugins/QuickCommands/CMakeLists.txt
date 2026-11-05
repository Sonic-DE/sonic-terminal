
kcoreaddons_add_plugin(konsole_quickcommandsplugin
SOURCES
    quickcommandsplugin.cpp
    quickcommandsbackend.cpp
    quickcommandsmodel.cpp
    filtermodel.cpp
INSTALL_NAMESPACE
    "konsoleplugins"
)

qt_add_resources(konsole_quickcommandsplugin "quickcommandspanel"
    PREFIX "/org/sonicde/terminal/plugins/quickcommands"
    FILES QuickCommandsPanel.qml
)

configure_file(konsole_quickcommands.in.json konsole_quickcommands.json)
target_link_libraries(konsole_quickcommandsplugin
    konsoleprivate
    konsoleapp
)
