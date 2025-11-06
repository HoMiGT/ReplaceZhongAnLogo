cmake_minimum_required(VERSION 3.16)

project(ReplaceZhongAnLogo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_TOOLCHAIN_FILE "$ENV{CMAKE_TOOLCHAIN_FILE}")
set(ENV{PKG_CONFIG_PATH} "$ENV{VCPKG_ROOT}/installed/x64-windows/lib/pkgconfig")

find_package(fmt REQUIRED)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(OpenCV_DIR "E:/3rdPackages/opencv/opencv/build")
find_package(OpenCV REQUIRED)

set(OpenCV_INCLUDE_DIR "E:/3rdPackages/opencv/opencv/build/include")
set(OpenCV_LIBRARY_DIR "E:/3rdPackages/opencv/opencv/build/x64/vc16/lib")
message(STATUS "OpenCV_INCLUDE_DIR: ${OpenCV_INCLUDE_DIR}")
message(STATUS "OpenCV_LIBRARY_DIR: ${OpenCV_LIBRARY_DIR}")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB OpenCV_LIBS "${OpenCV_LIBRARY_DIR}/opencv_world4100d.lib")
else ()
    file(GLOB OpenCV_LIBS "${OpenCV_LIBRARY_DIR}/opencv_world4100.lib")
endif ()

include_directories(${OpenCV_INCLUDE_DIR})

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(ReplaceZhongAnLogo
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        Icons.qrc
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET ReplaceZhongAnLogo APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(ReplaceZhongAnLogo SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(ReplaceZhongAnLogo
            ${PROJECT_SOURCES}
        )
    endif()
endif()



target_link_libraries(ReplaceZhongAnLogo PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    fmt::fmt
    ${OpenCV_LIBS}
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.ReplaceZhongAnLogo)
endif()
set_target_properties(ReplaceZhongAnLogo PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS ReplaceZhongAnLogo
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(ReplaceZhongAnLogo)
endif()
