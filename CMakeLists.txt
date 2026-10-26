
cmake_minimum_required(VERSION 3.20)

project(Calculator_cpp LANGUAGES CXX)

# Проект использует Windows API для загрузки DLL.
if(NOT WIN32)
    message(FATAL_ERROR "Calculator_cpp requires Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Куда складывать exe и dll.
# $<CONFIG> учитывает конфигурацию Visual Studio: Debug или Release.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")

# Основной калькулятор.
add_executable(calc
    src/main.cpp
    src/expr.cpp
    src/plugins.cpp
)

target_include_directories(calc PRIVATE include)

# Общая функция для сборки DLL-плагинов.
function(add_calculator_plugin target source output_name)
    add_library(${target} SHARED ${source})

    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${output_name}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/plugins"
    )
endfunction()

add_calculator_plugin(plugin_deg plugins/deg.cpp deg)
add_calculator_plugin(plugin_ln plugins/ln.cpp ln)
add_calculator_plugin(plugin_bad plugins/bad_two.cpp bad_two)

# Эти два плагина используют .def, чтобы экспортировать функции
# под именами sin и sqrt.
add_calculator_plugin(plugin_sin plugins/sin.cpp funcsin)
target_sources(plugin_sin PRIVATE vs/funcsin.def)

add_calculator_plugin(plugin_sqrt plugins/sqrt.cpp funcsqr)
target_sources(plugin_sqrt PRIVATE vs/funcsqrt.def)
