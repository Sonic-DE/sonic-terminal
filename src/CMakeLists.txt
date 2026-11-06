# cmake-options : -DCMAKE_DISABLE_FIND_PACKAGE_LibKonq=TRUE or FALSE; default is FALSE

add_definitions(-DTRANSLATION_DOMAIN=\"konsole\")

### Handle DragonFlyBSD here instead of using __DragonFly__
if(${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
  set(HAVE_OS_DRAGONFLYBSD 1)
else()
  set(HAVE_OS_DRAGONFLYBSD 0)
endif()

include(CheckIncludeFiles)
include(ECMAddAppIcon)

configure_file(config-konsole.h.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/config-konsole.h)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(XKBCOMMON QUIET xkbcommon)
    endif()
    if(XKBCOMMON_FOUND)
        message(STATUS "Found xkbcommon, enabling win32-input-mode support")
        set(KONSOLE_HAVE_XKBCOMMON TRUE)
    else()
        message(STATUS "xkbcommon not found — building without win32-input-mode support")
    endif()
else()
    message(STATUS "Not on Linux — building without win32-input-mode support")
endif()

### Tests
if(BUILD_TESTING)
        find_package(Qt6Test ${QT_MIN_VERSION} CONFIG REQUIRED)
        add_subdirectory(autotests)
        add_subdirectory(tests)
endif()

### Security concerns about reading arbitrary screen positions
option(ENABLE_DECRQCRA "Konsole: enable DEC request checksum rectangular area" OFF)

### Development tools
option(KONSOLE_BUILD_UNI2CHARACTERWIDTH "Konsole: build uni2characterwidth executable" OFF)
# Scripted UI driver for the parity tests (tools/parity); never enable for packages.
option(KONSOLE_PARITY_HOOKS "Konsole: build the --parity-script test hook" OFF)

set(dbus_xml_srcs)
if(HAVE_DBUS)
    ### Konsole source files shared between embedded terminal and main application
    # qdbuscpp2xml -m  Session.h -o org.kde.konsole.Session.xml
    # qdbuscpp2xml -M -s ViewManager.h -o org.kde.konsole.Konsole.xml

    # Generate dbus .xml files; do not store .xml in source folder
    qt_generate_dbus_interface(ViewManager.h org.kde.konsole.Window.xml OPTIONS -m)

    qt_add_dbus_adaptor(windowadaptors_SRCS
                        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Window.xml
                        ViewManager.h
                        Konsole::ViewManager)

    # qdbuscpp2xml -m  Session.h -o org.kde.konsole.Session.xml
    # Generate dbus .xml files; do not store .xml in source folder
    qt_generate_dbus_interface(session/Session.h org.kde.konsole.Session.xml OPTIONS -m)

    qt_add_dbus_adaptor(
        sessionadaptors_SRCS
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Session.xml
        session/Session.h
        Konsole::Session
    )

    # The same objects also answer to the SonicDE interface names.
    foreach(interface Window Session)
        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/org.sonicde.terminal.${interface}.xml
            COMMAND ${CMAKE_COMMAND}
                -DIN=${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.${interface}.xml
                -DOUT=${CMAKE_CURRENT_BINARY_DIR}/org.sonicde.terminal.${interface}.xml
                -DFROM=org.kde.konsole.${interface}
                -DTO=org.sonicde.terminal.${interface}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/RenameDBusInterface.cmake
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.${interface}.xml ${CMAKE_CURRENT_SOURCE_DIR}/RenameDBusInterface.cmake
            VERBATIM)
    endforeach()
    qt_add_dbus_adaptor(windowadaptors_SRCS
                        ${CMAKE_CURRENT_BINARY_DIR}/org.sonicde.terminal.Window.xml
                        ViewManager.h
                        Konsole::ViewManager
                        sonicwindowadaptor
                        SonicWindowAdaptor)
    qt_add_dbus_adaptor(sessionadaptors_SRCS
                        ${CMAKE_CURRENT_BINARY_DIR}/org.sonicde.terminal.Session.xml
                        session/Session.h
                        Konsole::Session
                        sonicsessionadaptor
                        SonicSessionAdaptor)

    set(dbus_xml_srcs
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Session.xml
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Window.xml
        ${CMAKE_CURRENT_BINARY_DIR}/org.sonicde.terminal.Session.xml
        ${CMAKE_CURRENT_BINARY_DIR}/org.sonicde.terminal.Window.xml
    )
endif()

set(konsole_LIBS
    KF6::XmlGui
    Qt::Multimedia
    Qt::PrintSupport
    Qt::Xml
    KF6::Notifications
    KF6::WindowSystem
    KF6::TextWidgets
    KF6::GuiAddons
    KF6::IconThemes
    KF6::Bookmarks
    KF6::I18n
    KF6::KIOWidgets
    KF6::NewStuffCore
)

if(NOT WIN32)
    list(APPEND konsole_LIBS
        KF6::Pty
    )
endif()

if(HAVE_DBUS)
    list(APPEND konsole_LIBS
        KF6::DBusAddons
        KF6::GlobalAccel
    )
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    #kinfo_getfile() is in libutil
    list(APPEND konsole_LIBS util)
endif()

if(HAIKU)
    list(APPEND konsole_LIBS network) # for undefined reference to `inet_aton'
endif()

# TODO: Move this to an internal folder if it grows too much.
# This konsoleprivate_core is the "core" library that depends in nothing from konsole itself
# so we can start to detangle the codebase.
set(konsoleprivate_core_SRCS
    ShellCommand.cpp
)

ecm_qt_declare_logging_category(
    konsoleprivate_core_SRCS
    HEADER konsoledebug.h
    IDENTIFIER KonsoleDebug
    CATEGORY_NAME org.kde.konsole
    DESCRIPTION "Konsole"
    EXPORT KONSOLE
)

add_library(konsoleprivate_core STATIC ${konsoleprivate_core_SRCS})
# Needed to link this static lib to shared libs
set_target_properties(konsoleprivate_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(konsoleprivate_core KF6::CoreAddons)

set(konsolehelpers_SRCS
    LabelsAligner.cpp
)
add_library(konsolehelpers
    STATIC
    ${konsolehelpers_SRCS}
)
set_target_properties(konsolehelpers PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(konsolehelpers
    Qt::Core
    Qt::Widgets
)

add_subdirectory(colorscheme)
add_subdirectory(keyboardtranslator)
add_subdirectory(characters)
add_subdirectory(decoders)
add_subdirectory(containers)
add_subdirectory(plugins)
if(WIN32)
    set(ptyqt_srcs ptyqt/iptyprocess.cpp ptyqt/conptyprocess.cpp)
endif()

set(konsoleprivate_SRCS ${windowadaptors_SRCS}
                        BookmarkHandler.cpp
                        BookmarkMenu.cpp
                        CheckableSessionModel.cpp
                        CopyInputDialog.cpp
                        Emulation.cpp
                        EscapeSequenceUrlExtractor.cpp
                        FontDialog.cpp
                        HistorySizeDialog.cpp
                        KeyBindingEditor.cpp
                        LabelsAligner.cpp
                        NullProcessInfo.cpp
                        PrintOptions.cpp
                        ProcessInfo.cpp
                        Pty.cpp
                        RenameTabDialog.cpp
                        SSHProcessInfo.cpp
                        SaveHistoryTask.cpp
                        SaveHistoryAutoTask.cpp
                        Screen.cpp
                        ScreenWindow.cpp
                        ScrollState.cpp
                        SearchHistoryTask.cpp
                        ShortcutStore.cpp
                        ShouldApplyProperty.cpp
                        UnixProcessInfo.cpp
                        ViewManager.cpp
                        ViewProperties.cpp
                        Vt102Emulation.cpp
                        WindowSystemInfo.cpp
                        WinKeys.h
                        ZModemDialog.cpp
                        filterHotSpots/EscapeSequenceUrlFilter.cpp
                        filterHotSpots/EscapeSequenceUrlFilterHotSpot.cpp
                        filterHotSpots/FileFilter.cpp
                        filterHotSpots/FileFilterHotspot.cpp
                        filterHotSpots/Filter.cpp
                        filterHotSpots/FilterChain.cpp
                        filterHotSpots/HotSpot.cpp
                        filterHotSpots/RegExpFilter.cpp
                        filterHotSpots/RegExpFilterHotspot.cpp
                        filterHotSpots/TerminalImageFilterChain.cpp
                        filterHotSpots/UrlFilter.cpp
                        filterHotSpots/UrlFilterHotspot.cpp
                        filterHotSpots/ColorFilter.cpp
                        filterHotSpots/ColorFilterHotSpot.cpp
                        history/HistoryFile.cpp
                        history/HistoryScroll.cpp
                        history/HistoryScrollFile.cpp
                        history/HistoryScrollNone.cpp
                        history/HistoryType.cpp
                        history/HistoryTypeFile.cpp
                        history/HistoryTypeNone.cpp
                        history/compact/CompactHistoryScroll.cpp
                        history/compact/CompactHistoryType.cpp
                        widgets/DetachableTabBar.cpp
                        widgets/EditProfileDialog.cpp
                        widgets/HistorySizeWidget.cpp
                        widgets/IncrementalSearchBar.cpp
                        widgets/IncrementalSearchBarSizeGrip.cpp
                        widgets/RenameTabWidget.cpp
                        widgets/TabTitleFormatButton.cpp

                        searchtabs/SearchTabs.cpp
                        searchtabs/SearchTabsModel.cpp

                        terminalDisplay/extras/CompositeWidgetFocusWatcher.cpp
                        terminalDisplay/extras/AutoScrollHandler.cpp
                        terminalDisplay/extras/HighlightScrolledLines.cpp

                        terminalDisplay/TerminalDisplay.cpp
                        terminalDisplay/TerminalPainter.cpp
                        quick/TerminalView.cpp
                        quick/TerminalViewRegistry.cpp
                        quick/TerminalViewAccessible.cpp
                        quick/ChromeAdapter.cpp
                        quick/CompatWindows.cpp
                        quick/QmlDialogs.cpp
                        quick/FrameworkDialogScope.cpp
                        quick/CopyInputModel.cpp
                        quick/DialogHelpers.cpp
                        quick/ToolBarEditorForeign.h
                        quick/ColorSchemeEditorBackend.cpp
                        quick/KeyBindingEditorBackend.cpp
                        quick/ShortcutsEditorModel.cpp
                        quick/CommandBarModel.cpp
                        quick/ProfileEditor.cpp
                        quick/ProfileListModel.cpp
                        quick/SettingsEditor.cpp
                        quick/DockAdapter.cpp
                        quick/FindBarAdapter.cpp
                        quick/HeaderBarAdapter.cpp
                        quick/LabelAdapter.cpp
                        quick/MessageFrameItem.cpp
                        quick/MessagesAdapter.cpp
                        quick/PaneTreeAdapter.cpp
                        quick/TabBarAdapter.cpp
                        quick/ToolButtonsAdapter.cpp
                        quick/PixmapItem.cpp
                        quick/ScrollBarAdapter.cpp
                        quick/SearchTabsAdapter.cpp
                        quick/StyleElement.cpp
                        quick/WidgetMirror.cpp
                        model/PaneModel.cpp
                        SonicLoggingCore.cpp
                        terminalDisplay/TerminalScrollBar.cpp
                        terminalDisplay/TerminalColor.cpp
                        terminalDisplay/TerminalFonts.cpp
                        terminalDisplay/TerminalBell.cpp

                        profile/Profile.cpp
                        profile/ProfileCommandParser.cpp
                        profile/ProfileGroup.cpp
                        profile/ProfileList.cpp
                        profile/ProfileReader.cpp
                        profile/ProfileWriter.cpp
                        profile/ProfileManager.cpp
                        profile/ProfileModel.cpp

                        ${ptyqt_srcs} # windows conpty backend

                        ${sessionadaptors_SRCS}
                        session/Session.cpp
                        session/SessionController.cpp
                        session/SessionDisplayConnection.cpp
                        session/SessionGroup.cpp
                        session/SessionListModel.cpp
                        session/SessionManager.cpp
                        session/SessionTask.cpp

                        widgets/TerminalDisplayAccessible.cpp
                        widgets/TerminalHeaderBar.cpp
                        widgets/ViewContainer.cpp
                        widgets/ViewSplitter.cpp
                        widgets/PaneWidgetView.cpp
                        widgets/KonsolePrintManager.cpp

                        ${dbus_xml_srcs}

                        ../data/data.qrc
                        ../desktop/konsole.qrc)

kconfig_add_kcfg_files(konsoleprivate_SRCS settings/KonsoleSettings.kcfgc)

### Konsole Application

ki18n_wrap_ui(konsoleprivate_SRCS
    CopyInputDialog.ui
    HistorySizeDialog.ui
    KeyBindingEditor.ui
    PrintOptions.ui       # Temporary to build session static library
    RenameTabDialog.ui
    settings/GeneralSettings.ui
    settings/MemorySettings.ui
    settings/PartInfo.ui
    settings/ProfileSettings.ui
    settings/TabBarSettings.ui
    settings/TemporaryFilesSettings.ui
    settings/ThumbnailsSettings.ui
    widgets/EditProfileAdvancedPage.ui
    widgets/EditProfileAppearancePage.ui
    widgets/EditProfileGeneralPage.ui
    widgets/EditProfileKeyboardPage.ui
    widgets/EditProfileMousePage.ui
    widgets/EditProfileScrollingPage.ui
    widgets/EditProfileTabsPage.ui
    widgets/HistorySizeWidget.ui
    widgets/RenameTabWidget.ui
)

add_library(konsoleprivate ${konsoleprivate_SRCS})
generate_export_header(konsoleprivate BASE_NAME konsoleprivate)

# The QML terminal: loaded from resources until phase 5 installs the module.
qt_add_qml_module(konsoleprivate
    URI org.sonicde.terminal
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/org/sonicde/terminal
    QML_FILES
        qml/ActionToolButton.qml
        qml/ClearableTextField.qml
        qml/ColorButton.qml
        qml/ColorComboBox.qml
        qml/ColorSchemeEditorDialog.qml
        qml/CommandBar.qml
        qml/ConfigureDialog.qml
        qml/ConfigureGeneralPage.qml
        qml/ConfigureMemoryPage.qml
        qml/ConfigureProfilesPage.qml
        qml/ConfigureTabBarPage.qml
        qml/ConfigureTemporaryFilesPage.qml
        qml/ConfigureThumbnailsPage.qml
        qml/ConfigureToolbarsDialog.qml
        qml/CopyInputDialog.qml
        qml/DialogPage.qml
        qml/DialogWindow.qml
        qml/DockPanels.qml
        qml/EditProfileAdvancedPage.qml
        qml/EditProfileAppearancePage.qml
        qml/EditProfileDialog.qml
        qml/EditProfileGeneralPage.qml
        qml/EditProfileKeyboardPage.qml
        qml/EditProfileMousePage.qml
        qml/EditProfileScrollingPage.qml
        qml/EditProfileTabsPage.qml
        qml/HistorySizeDialog.qml
        qml/HistorySizeFields.qml
        qml/IconTextDialog.qml
        qml/KeyBindingEditorDialog.qml
        qml/MnemonicLabel.qml
        qml/PageDialog.qml
        qml/PanelTree.qml
        qml/PartInfoPage.qml
        qml/PartSettingsDialog.qml
        qml/ProfileCheckBox.qml
        qml/ProfileRadioButton.qml
        qml/RenameTabDialog.qml
        qml/SettingCheckBox.qml
        qml/SettingRadioButton.qml
        qml/SettingSpinBox.qml
        qml/ShortcutsDialog.qml
        qml/TabbedPageDialog.qml
        qml/TabTitleFields.qml
        qml/TextInputDialog.qml
        qml/PaneTree.qml
        qml/SearchTabs.qml
        qml/TabBar.qml
        qml/TerminalPane.qml
        qml/ToolBarLine.qml
)
# The generated type registrations include QML_ELEMENT headers by file name.
target_include_directories(konsoleprivate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/quick)

if(KONSOLE_HAVE_XKBCOMMON)
    target_compile_definitions(konsoleprivate PUBLIC HAVE_XKBCOMMON)
    target_include_directories(konsoleprivate PUBLIC ${XKBCOMMON_INCLUDE_DIRS})
    target_link_libraries(konsoleprivate PRIVATE ${XKBCOMMON_LIBRARIES})
endif()

target_link_libraries(konsoleprivate
    PUBLIC
    Qt::Quick
    Qt::QuickWidgets
    konsoleprivate_core
    konsolecolorscheme
    keyboardtranslator
    konsolehelpers
    konsolecharacters
    konsoledecoders
    konsolecontainers
    KF6::NewStuffCore
    KF6::NewStuffWidgets
    ${konsole_LIBS}
    ICU::uc
    ICU::i18n
)

target_link_libraries(konsoleprivate
    PRIVATE
    Sonic::Logging
    KF6::IconWidgets
    KF6::BookmarksWidgets
    KF6::I18nQml
    Qt::QuickControls2
)

set_target_properties(konsoleprivate PROPERTIES
    VERSION ${RELEASE_SERVICE_VERSION}
)

install(TARGETS konsoleprivate ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)

add_library(konsoleapp SHARED Application.cpp
    MainWindow.cpp
    settings/ConfigurationDialog.cpp
    settings/ConfigDialogButtonGroupManager.cpp
    settings/TemporaryFilesSettings.cpp
    settings/GeneralSettings.cpp
    settings/MemorySettings.cpp
    settings/ProfileSettings.cpp
    settings/TabBarSettings.cpp
    settings/ThumbnailsSettings.cpp
    pluginsystem/IKonsolePlugin.cpp
    pluginsystem/PluginManager.cpp
    delegates/ProfileShortcutDelegate.cpp
    SonicLoggingApp.cpp
    quick/WindowController.cpp
    quick/TerminalHost.cpp
    quick/PanelOpen.cpp
)

generate_export_header(konsoleapp BASE_NAME konsoleapp)

qt_add_qml_module(konsoleapp
    URI org.sonicde.terminal.app
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/org/sonicde/terminal/app
    QML_FILES
        qml/Main.qml
        qml/TerminalArea.qml
        qml/TerminalPanelArea.qml
    IMPORTS
        org.sonicde.terminal
)

if(HAVE_DBUS)
    target_sources(konsoleapp PRIVATE quick/MainWindowInterface.cpp)
endif()

target_compile_definitions(konsoleapp PRIVATE -DRELEASE_SERVICE_VERSION="${RELEASE_SERVICE_VERSION}")

if(KONSOLE_PARITY_HOOKS)
    find_package(Qt6Test ${QT_MIN_VERSION} CONFIG REQUIRED)
    target_sources(konsoleapp PRIVATE parity/ParityHook.cpp)
    target_compile_definitions(konsoleapp PUBLIC KONSOLE_PARITY_HOOKS=1)
    # The phase 0 copy has no QML window.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/quick/WindowController.h)
        target_sources(konsoleapp PRIVATE parity/ParityHookQml.cpp)
        target_compile_definitions(konsoleapp PRIVATE KONSOLE_PARITY_QML=1)
    endif()
    target_link_libraries(konsoleapp PUBLIC Qt::Test)
    add_subdirectory(parity/compatplugin)
endif()

target_include_directories(konsoleapp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/quick)

target_link_libraries(konsoleapp PRIVATE Sonic::Ipc)
target_link_libraries(konsoleapp PUBLIC
  konsoleprivate
  Sonic::Logging
  Qt::Qml
  Qt::Quick
  Qt::QuickControls2
  KF6::XmlGui
  KF6::WindowSystem
  KF6::Bookmarks
  KF6::I18n
  KF6::KIOWidgets
  KF6::NotifyConfig
  KF6::Crash
)

set_target_properties(konsoleapp PROPERTIES
    VERSION ${RELEASE_SERVICE_VERSION}
)

install(TARGETS konsoleapp ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)

# The QML modules, for other programs: their QML is in the libraries' resources.
# The engine looks for lib<name>.so; KDECMakeSettings takes the prefix off modules.
set_target_properties(konsoleprivateplugin konsoleappplugin PROPERTIES PREFIX "lib")
install(TARGETS konsoleprivateplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/sonicde/terminal)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/qml/org/sonicde/terminal/qmldir
    ${CMAKE_CURRENT_BINARY_DIR}/qml/org/sonicde/terminal/konsoleprivate.qmltypes
    DESTINATION ${KDE_INSTALL_QMLDIR}/org/sonicde/terminal)
install(TARGETS konsoleappplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/sonicde/terminal/app)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/qml/org/sonicde/terminal/app/qmldir
    ${CMAKE_CURRENT_BINARY_DIR}/qml/org/sonicde/terminal/app/konsoleapp.qmltypes
    DESTINATION ${KDE_INSTALL_QMLDIR}/org/sonicde/terminal/app)

set(konsole_SRCS
    main.cpp
)

# Sets the icon on Windows and OSX
ecm_add_app_icon(ICONS_SOURCES ICONS ${ICONS_SRCS})

add_executable(konsole ${konsole_SRCS} ${ICONS_SOURCES})
target_link_libraries(konsole
  konsoleprivate
  konsoleapp
  Sonic::Logging
  KF6::XmlGui
  KF6::WindowSystem
  KF6::Bookmarks
  KF6::I18n
  KF6::KIOWidgets
  KF6::NotifyConfig
  KF6::Crash
)

if(APPLE)
    set_target_properties(konsole PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.konsole"
        MACOSX_BUNDLE_BUNDLE_NAME "Konsole"
        MACOSX_BUNDLE_DISPLAY_NAME "Konsole"
        MACOSX_BUNDLE_INFO_STRING "Konsole, the KDE terminal emulator"
        MACOSX_BUNDLE_LONG_VERSION_STRING "Konsole ${RELEASE_SERVICE_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${RELEASE_SERVICE_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "1997-2022 The Konsole Developers")
endif()

install(TARGETS konsole ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

### Embedded Konsole KPart

set(konsolepart_PART_SRCS Part.cpp
    settings/PartInfo.cpp
    settings/ProfileSettings.cpp
    delegates/ProfileShortcutDelegate.cpp
)

add_library(konsolepart MODULE ${konsolepart_PART_SRCS})
generate_export_header(konsolepart BASE_NAME konsole)
set_target_properties(konsolepart PROPERTIES DEFINE_SYMBOL KONSOLE_PART)
target_link_libraries(konsolepart
    KF6::Parts
    KF6::XmlGui
    konsoleprivate
)

install(TARGETS konsolepart DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/parts)

# Add cmake-options -DBUILD_COVERAGE=ON -DCOVERAGE_GENERATE_HTML=ON
if(COVERAGE_GENERATE_HTML)
  # find required tools - no error checking
  find_program(LCOV lcov REQUIRED)
  find_program(GENHTML genhtml REQUIRED)

  add_custom_target(coverage
    # runs tests to get .gcda files
    COMMAND ctest --progress
    # generate data
    COMMAND ${LCOV} --directory . --capture --output-file coverage.info
    # generate report
    COMMAND ${GENHTML} --ignore-errors source --demangle-cpp -o coverage coverage.info
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

