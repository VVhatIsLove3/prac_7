
cmake_minimum_required(VERSION 3.10)
project(my_project)

set(CMAKE_C_STANDARD 11)

# Добавляем исполняемые файлы для каждой программы
find_package(Threads REQUIRED)
link_libraries(Threads::Threads)
add_executable(7_1 7_1.c)
add_executable(7_2 7_2.c)

# можно добавить их как поддиректории
if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/standart)
    add_subdirectory(standart)
endif()

if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/withi)
    add_subdirectory(withi)
endif()
