cmake_minimum_required(VERSION 3.16)
project(Minesweeper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

add_executable(MineSweeper
    resources.qrc
    src/main.cpp
    src/game/Board.cpp
    src/game/Statistics.cpp
    src/ui/MainWindow.cpp
    src/ui/BoardWidget.cpp
    src/ui/AppearanceDialog.cpp
    src/ui/HelpDialog.cpp
    src/ui/OptionsDialog.cpp
    src/ui/ResultDialog.cpp
    src/ui/StatisticsDialog.cpp
)

target_include_directories(MineSweeper PRIVATE
    src
    src/game
    src/ui
)

target_link_libraries(MineSweeper PRIVATE Qt6::Widgets Qt6::Multimedia)

install(TARGETS MineSweeper RUNTIME DESTINATION bin)
install(FILES linux-minesweeper.desktop DESTINATION share/applications)

# The Windows 7 mine as the desktop icon: the authentic 32px frame from
# the original .ico, the game's own high-resolution mine art above that.
foreach(size 32 48 64 128)
    install(FILES assets/icons/MineSweeper-${size}.png
            DESTINATION share/icons/hicolor/${size}x${size}/apps
            RENAME MineSweeper.png)
endforeach()
