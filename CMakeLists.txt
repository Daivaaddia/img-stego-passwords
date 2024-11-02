cmake_minimum_required(VERSION 3.15)

project(stego_png VERSION 0.1 DESCRIPTION LANGUAGES CXX)

include_directories(include)

add_executable(stegopng stego.cpp app.cpp)

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

target_link_libraries(stegopng ZLIB::ZLIB OpenSSL::SSL OpenSSL::Crypto)

target_compile_features(stegopng PRIVATE cxx_std_17)