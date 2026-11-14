cmake_minimum_required(VERSION 3.21)

project(WardenBrowser
    VERSION 0.1.0
    DESCRIPTION "Warden Browser - private cross-platform desktop web browser"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    WebEngineWidgets
    WebEngineCore
    Network
    Sql
)

find_package(OpenSSL REQUIRED)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.h"
)

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
    ${SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebEngineCore
    Qt6::Network
    Qt6::Sql
    OpenSSL::Crypto
)

# ---------------------------------------------------------------------------
# macOS bundle properties
# ---------------------------------------------------------------------------
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Warden Browser"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.vakarman-dev.wardenbrowser"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
endif()

# ---------------------------------------------------------------------------
# Packaging (CPack)
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "WardenBrowser")
set(CPACK_PACKAGE_VENDOR "vakarman-dev")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Warden Browser - private desktop web browser")
set(CPACK_PACKAGE_CONTACT "vakarman-dev")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "WardenBrowser")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "Warden Browser")
    set(CPACK_NSIS_PACKAGE_NAME "Warden Browser")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "WardenBrowser.exe")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/warden.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/warden.ico")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
elseif(UNIX)
    if(NOT APPLE)
        set(CPACK_GENERATOR "DEB")
        set(CPACK_DEBIAN_PACKAGE_NAME "wardenbrowser")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "vakarman-dev")
        set(CPACK_DEBIAN_PACKAGE_SECTION "net")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/vakarman-dev/warden-browser")
    else()
        set(CPACK_GENERATOR "DragNDrop")
    endif()
endif()

include(CPack)
