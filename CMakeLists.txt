cmake_minimum_required(VERSION 3.20)
project(MinimalExample VERSION 1.0.0 LANGUAGES C CXX)

# Платформенный бэкенд движка для Apple содержит .mm-файлы (Objective-C++).
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Подключаем CrossRender как движок-библиотеку: собственные пример и тесты
# движка не нужны, чтобы не собирать витрину из ~40 сцен зря.
set(CR_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(CR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(crossrender)

add_executable(MinimalExample src/main.cpp)
target_link_libraries(MinimalExample PRIVATE crossrender)

if(MSVC)
    # В исходнике есть кириллические литералы — без /utf-8 MSVC читает файл в кодировке локали.
    target_compile_options(MinimalExample PRIVATE /utf-8 /W3)
else()
    target_compile_options(MinimalExample PRIVATE -Wall -Wextra)
endif()

# Бинарник всегда кладём в build/bin: движок ищет каталог assets, поднимаясь
# от исполняемого файла, поэтому <корень проекта>/assets находится сам.
set_target_properties(MinimalExample PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin)

if(EMSCRIPTEN)
    set_target_properties(MinimalExample PROPERTIES SUFFIX ".html")
    target_link_options(MinimalExample PRIVATE
        "--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXIT_RUNTIME=1"
        "-sENVIRONMENT=web")
endif()
