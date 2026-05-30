cmake_minimum_required(VERSION 3.16)
project(QSettingsHelper VERSION 1.0 LANGUAGES CXX)

# ──────────────────────────────────────────────────────────────────────────────
# TemplateHelper companion toolkit (sibling directory)
# ──────────────────────────────────────────────────────────────────────────────
if (NOT TARGET TemplateHelper)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../TemplateHelper" TemplateHelper_build)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Qt — auto-detects Qt6 (preferred) or Qt5
# ──────────────────────────────────────────────────────────────────────────────
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

# ──────────────────────────────────────────────────────────────────────────────
# QSettingsHelper — header-only INTERFACE target
#
# Usage:
#   target_link_libraries(MyTarget PRIVATE QSettingsHelper::QSettingsHelper)
# ──────────────────────────────────────────────────────────────────────────────
add_library(QSettingsHelper INTERFACE
    qsettings_helper.h
)
add_library(QSettingsHelper::QSettingsHelper ALIAS QSettingsHelper)

target_compile_features(QSettingsHelper INTERFACE cxx_std_20)
target_include_directories(QSettingsHelper INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# TemplateHelper::Qt already propagates Qt::Core transitively.
target_link_libraries(QSettingsHelper INTERFACE
    TemplateHelper::Qt
)
