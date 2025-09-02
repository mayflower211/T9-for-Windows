cmake_minimum_required(VERSION 3.10)

project(T9_for_Windows LANGUAGES CXX RC)

add_compile_options(/utf-8)

# --- НОВАЯ СЕКЦИЯ ---
# Устанавливаем стандарт C++ для нашего проекта
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# --------------------

add_executable(t9-app WIN32 
    src/main.cpp 
    src/T9Engine.cpp
    resources.rc 
)

set_target_properties(t9-app PROPERTIES COMPILE_DEFINITIONS "_UNICODE;UNICODE")

target_link_libraries(t9-app PRIVATE user32.lib gdi32.lib shell32.lib comctl32.lib)