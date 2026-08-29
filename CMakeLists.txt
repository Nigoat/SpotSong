# Copyright (C) 2026 SpotSong Contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.16)

project(SpotSong VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Multimedia
    Sql
    Network
)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(TAGLIB taglib)
endif()

if(NOT TAGLIB_FOUND)
    find_path(TAGLIB_INCLUDE_DIRS NAMES taglib/tag.h)
    find_library(TAGLIB_LIBRARIES NAMES tag)
    if(TAGLIB_INCLUDE_DIRS AND TAGLIB_LIBRARIES)
        set(TAGLIB_FOUND TRUE)
    endif()
endif()

set(SOURCES
    src/main.cpp
    src/core/DatabaseManager.h
    src/core/DatabaseManager.cpp
    src/core/TagReader.h
    src/core/TagReader.cpp
    src/core/AudioPlayer.h
    src/core/AudioPlayer.cpp
    src/core/MusicLibrary.h
    src/core/MusicLibrary.cpp
    src/core/PlaylistManager.h
    src/core/PlaylistManager.cpp
    src/core/SystemInfo.h
    src/core/SystemInfo.cpp
    src/core/DiscordRPC.h
    src/core/DiscordRPC.cpp
    src/tui/TerminalUI.h
    src/tui/TerminalUI.cpp
)

add_executable(spotsong ${SOURCES})

target_include_directories(spotsong PRIVATE
    src
    src/core
    src/tui
)

if(TAGLIB_FOUND)
    target_include_directories(spotsong PRIVATE ${TAGLIB_INCLUDE_DIRS})
    target_link_libraries(spotsong PRIVATE ${TAGLIB_LIBRARIES})
else()
    target_link_libraries(spotsong PRIVATE tag)
endif()

target_link_libraries(spotsong PRIVATE
    Qt6::Core
    Qt6::Multimedia
    Qt6::Sql
    Qt6::Network
)

install(TARGETS spotsong
    RUNTIME DESTINATION bin
)
