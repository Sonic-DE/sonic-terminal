
ecm_qt_declare_logging_category(
    extra_sshplugin_SRCS
    HEADER sshmanagerplugindebug.h
    IDENTIFIER SshManagerPluginDebug
    CATEGORY_NAME org.kde.konsole.plugin.sshmanager
    DESCRIPTION "Konsole plugin (SSH Manager)"
    DEFAULT_SEVERITY Warning
    EXPORT KONSOLE
)

kcoreaddons_add_plugin(konsole_sshmanagerplugin
SOURCES
    sshmanagerplugin.cpp
    sshmanagerbackend.cpp
    sshmanagermodel.cpp
    sshmanagerfiltermodel.cpp
    ${extra_sshplugin_SRCS}
INSTALL_NAMESPACE
    "konsoleplugins"
)

qt_add_resources(konsole_sshmanagerplugin "sshmanagerpanel"
    PREFIX "/org/sonicde/terminal/plugins/sshmanager"
    FILES SSHManagerPanel.qml
)

configure_file(konsole_sshmanager.in.json konsole_sshmanager.json)
target_link_libraries(konsole_sshmanagerplugin
    konsoleprivate
    konsoleapp
)
