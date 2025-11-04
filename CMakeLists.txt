cmake_minimum_required(VERSION 3.16)

project(EECS482_P1)
set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

add_library(libthread STATIC IMPORTED)
if (APPLE)
    add_compile_options(-D_XOPEN_SOURCE)
    set_target_properties(libthread PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/libthread_macos.o)
else ()
    set_target_properties(libthread PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/libthread.o)
endif ()

# Build target (customize this with the source files for your program)
add_executable(pizza file1.cpp file2.cpp)
target_link_libraries(pizza libthread ${CMAKE_DL_LIBS} Threads::Threads)
add_custom_command(POST_BUILD TARGET pizza COMMAND ${CMAKE_CURRENT_LIST_DIR}/autotag.sh push)
