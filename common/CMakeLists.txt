add_library(colibry_common STATIC
    colibry/Keys.h
    colibry/SettingsStore.h
    colibry/SettingsStore.cpp
    colibry/ThemeManager.h
    colibry/ThemeManager.cpp
    colibry/ThemeSync.h
    colibry/ThemeSync.cpp
)
target_include_directories(colibry_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(colibry_common PUBLIC Qt6::Widgets)
colibry_target_defaults(colibry_common)

if(Colibry_BUILD_TESTS)
    colibry_add_test(common_settings_store SOURCES tests/test_settings_store.cpp LINK colibry_common)
    colibry_add_test(common_theme_sync SOURCES tests/test_theme_sync.cpp LINK colibry_common)
endif()
