cmake_minimum_required(VERSION 3.16)
project(Minesweeper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

add_executable(minesweeper
    resources.qrc
    src/main.cpp
    src/game/Board.cpp
    src/game/Statistics.cpp
    src/ui/MainWindow.cpp
    src/ui/BoardWidget.cpp
    src/ui/AppearanceDialog.cpp
    src/ui/OptionsDialog.cpp
    src/ui/ResultDialog.cpp
    src/ui/StatisticsDialog.cpp
)

target_include_directories(minesweeper PRIVATE
    src
    src/game
    src/ui
)

target_link_libraries(minesweeper PRIVATE Qt6::Widgets Qt6::Multimedia)

install(TARGETS minesweeper RUNTIME DESTINATION bin)
install(FILES linux-minesweeper.desktop DESTINATION share/applications)
