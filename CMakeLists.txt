cmake_minimum_required(VERSION 3.20)
project(QuickLookLinux VERSION 1.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets Pdf PdfWidgets Network)

add_executable(quicklook-linux
    src/main.cpp
    src/PreviewWindow.cpp
    src/PreviewWindow.h
    resources.qrc
)

target_link_libraries(quicklook-linux PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Pdf
    Qt6::PdfWidgets
    Qt6::Network
)

install(TARGETS quicklook-linux RUNTIME DESTINATION bin)

# Publish real icon sizes instead of forcing Plasma to downscale the 512 px asset.
install(FILES assets/quicklook-linux-512.png
        RENAME quicklook-linux.png
        DESTINATION share/icons/hicolor/512x512/apps)
install(FILES assets/quicklook-linux-256.png
        RENAME quicklook-linux.png
        DESTINATION share/icons/hicolor/256x256/apps)
install(FILES assets/quicklook-linux-128.png
        RENAME quicklook-linux.png
        DESTINATION share/icons/hicolor/128x128/apps)
install(FILES assets/quicklook-linux-64.png
        RENAME quicklook-linux.png
        DESTINATION share/icons/hicolor/64x64/apps)
install(FILES assets/quicklook-linux-48.png
        RENAME quicklook-linux.png
        DESTINATION share/icons/hicolor/48x48/apps)
install(FILES assets/quicklook-linux-32.png
        RENAME quicklook-linux.png
        DESTINATION share/icons/hicolor/32x32/apps)
