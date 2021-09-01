cmake_minimum_required(VERSION 3.20)
project(Parallel_grep)

find_package(LibArchive REQUIRED)
find_package(TBB REQUIRED)
find_package(Threads REQUIRED)
set (Boost_USE_STATIC_LIBS OFF)
set (Boost_USE_MULTITHREAD ON)
find_package( Boost REQUIRED COMPONENTS filesystem)

set(CMAKE_CXX_STANDARD 14)

add_executable(Parallel_grep main.cpp)

target_link_libraries(Parallel_grep Threads::Threads ${LibArchive_LIBRARIES})
target_link_libraries(Parallel_grep tbb Boost::filesystem)
