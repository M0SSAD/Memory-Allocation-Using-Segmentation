# 1. Version Requirement
cmake_minimum_required(VERSION 3.15)

# 2. Project Metadata
project(CoreSystem LANGUAGES CXX)

# 3. Target Declaration & 4. Source Mapping
add_executable(app src/main.cpp src/Memory.cpp)

# 5. Include Path Resolution
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)