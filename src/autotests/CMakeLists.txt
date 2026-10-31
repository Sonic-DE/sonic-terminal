include(ECMAddTests)

if(KONSOLE_HAVE_XKBCOMMON)
    add_compile_definitions(HAVE_XKBCOMMON)

    list(APPEND KONSOLE_TEST_LIBS ${XKBCOMMON_LIBRARIES})
endif()

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

set(KONSOLE_TEST_LIBS Qt::Test konsoleprivate konsolecharacters konsoledecoders konsoleapp)

if (CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    set(KONSOLE_TEST_LIBS ${KONSOLE_TEST_LIBS} kvm)
endif()

# Temporary bypass for BKO 432379
if(BUILD_SHARED_LIBS)
    ecm_add_test(KeyboardTranslatorTest.cpp LINK_LIBRARIES ${KONSOLE_TEST_LIBS})
endif()

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT WIN32)
    ecm_add_test(
        PartTest.cpp
        LINK_LIBRARIES KF6::XmlGui KF6::Parts KF6::Pty ${KONSOLE_TEST_LIBS}
    )
    ecm_add_test(
        PartCompatTest.cpp
        LINK_LIBRARIES KF6::XmlGui KF6::Parts Qt::Xml Qt::Test
    )
    target_compile_definitions(PartCompatTest PRIVATE
        PARTUI_RC="${CMAKE_SOURCE_DIR}/desktop/partui.rc"
        PARTCOMPAT_HOST_RC="${CMAKE_CURRENT_SOURCE_DIR}/data/partcompat-host.rc")
    set_tests_properties(PartCompatTest PROPERTIES LABELS compat ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

ecm_add_tests(
    BookMarkTest.cpp
    CharacterColorTest.cpp
    CharacterTest.cpp
    CharacterWidthTest.cpp
    HotSpotFilterTest.cpp
    ProcessInfoTest.cpp
    ProfileTest.cpp
    ScreenTest.cpp
    ShellCommandTest.cpp
    StringConverterTest.cpp
    TerminalCharacterDecoderTest.cpp
    Vt102EmulationTest.cpp
    LINK_LIBRARIES ${KONSOLE_TEST_LIBS}
)

if(NOT WIN32)
    ecm_add_tests(
        PtyTest.cpp
        LINK_LIBRARIES KF6::Pty ${KONSOLE_TEST_LIBS}
    )
endif()

ecm_add_tests(
    HistoryTest.cpp
    SessionTest.cpp
    TerminalInterfaceTest.cpp
    TerminalTest.cpp
    ViewManagerTest.cpp
    BlurTest.cpp
    PaneModelTest.cpp
    TerminalViewTest.cpp
    QmlWindowTest.cpp
    LINK_LIBRARIES ${KONSOLE_TEST_LIBS} KF6::Parts
)
set_tests_properties(QmlWindowTest PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORMTHEME=kde;QT_STYLE_OVERRIDE=Silver;QT_QUICK_CONTROLS_STYLE=org.kde.desktop")
# As the parity runs: widgets and the desktop style's QML items read one colour scheme.
set_tests_properties(TerminalViewTest PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORMTHEME=kde;QT_STYLE_OVERRIDE=Silver;QT_QUICK_CONTROLS_STYLE=org.kde.desktop")

if(HAVE_DBUS)
    find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)
    find_program(XVFB_EXECUTABLE Xvfb REQUIRED)
    add_executable(DBusCodecTest DBusCodecTest.cpp)
    target_compile_definitions(DBusCodecTest PRIVATE
        KONSOLE_EXECUTABLE="$<TARGET_FILE:konsole>"
        XVFB_EXECUTABLE="${XVFB_EXECUTABLE}"
    )
    target_link_libraries(DBusCodecTest Qt::Core Qt::DBus Qt::Test)
    add_dependencies(DBusCodecTest konsole)
    add_test(NAME DBusCodecTest COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} $<TARGET_FILE:DBusCodecTest>)
endif()

# The parity comparison must reject each kind of broken run (tools/parity/README.md).
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME ParityCompareSelfTest COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/parity/selftest.py)
endif()
