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
    LINK_LIBRARIES ${KONSOLE_TEST_LIBS} KF6::Parts
)

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
