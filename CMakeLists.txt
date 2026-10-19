cmake_minimum_required(VERSION 3.20)
project(MultiMediaPlayer VERSION 1.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Widgets Multimedia MultimediaWidgets
)

find_package(Qt6 OPTIONAL_COMPONENTS Pdf PdfWidgets)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp src/MainWindow.h
    src/MediaPanel.cpp src/MediaPanel.h
    src/VideoPanel.cpp src/VideoPanel.h
    src/AudioPanel.cpp src/AudioPanel.h
    src/ImagePanel.cpp src/ImagePanel.h
    src/PdfPanel.cpp src/PdfPanel.h
    src/TextPanel.cpp src/TextPanel.h
)

add_executable(MultiMediaPlayer WIN32 ${SOURCES})

target_link_libraries(MultiMediaPlayer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets
    Qt6::Multimedia Qt6::MultimediaWidgets
)

if(Qt6Pdf_FOUND)
    target_link_libraries(MultiMediaPlayer PRIVATE Qt6::Pdf Qt6::PdfWidgets)
    target_compile_definitions(MultiMediaPlayer PRIVATE HAS_QTPDF)
endif()

if(WIN32)
    set_target_properties(MultiMediaPlayer PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

qt_finalize_executable(MultiMediaPlayer)