# Shared CMake helpers for every Colibry application.

# Warning level, string-safety defines and the version macro for one target.
function(colibry_target_defaults target)
    target_compile_definitions(${target} PUBLIC
        Colibry_APPS_VERSION="${PROJECT_VERSION}"
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
        QT_NO_FOREACH)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(Colibry_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(Colibry_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# colibry_add_test(<name> SOURCES <files...> LINK <libs...>)
# Builds a QtTest executable and registers it with CTest (offscreen platform).
function(colibry_add_test name)
    cmake_parse_arguments(T "" "" "SOURCES;LINK" ${ARGN})
    add_executable(${name} ${T_SOURCES})
    target_link_libraries(${name} PRIVATE ${T_LINK} Qt6::Test)
    colibry_target_defaults(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_LOGGING_RULES=*.debug=false")
endfunction()

# colibry_install_app(<target> <desktop-file> <icon-svg>)
function(colibry_install_app target desktop icon)
    include(GNUInstallDirs)
    install(TARGETS ${target} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${desktop} DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES ${icon}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endfunction()
