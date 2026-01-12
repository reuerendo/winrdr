cmake_minimum_required(VERSION 3.15)
project(EpubReader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    add_compile_options(/utf-8)
endif()

# Gumbo-parser
file(GLOB GUMBO_SOURCES "libs/gumbo-parser/src/*.c")

if(NOT GUMBO_SOURCES)
    message(FATAL_ERROR "No gumbo-parser source files found in libs/gumbo-parser/src/")
endif()

add_library(gumbo STATIC ${GUMBO_SOURCES})
target_include_directories(gumbo PUBLIC libs/gumbo-parser/src)

if(MSVC)
    target_compile_definitions(gumbo PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Litehtml
file(GLOB_RECURSE LITEHTML_SOURCES "libs/litehtml/src/*.cpp")

if(NOT LITEHTML_SOURCES)
    message(FATAL_ERROR "No litehtml source files found in libs/litehtml/src/")
endif()

add_library(litehtml STATIC ${LITEHTML_SOURCES})
target_include_directories(litehtml PUBLIC libs/litehtml/include)
target_link_libraries(litehtml PUBLIC gumbo)

if(MSVC)
    target_compile_options(litehtml PRIVATE /W3)
endif()

# Main application
set(SOURCES
    src/main.cpp
    src/epub/epub_parser.cpp
    src/epub/zip_handler.cpp
    src/epub/image_cache.cpp
    src/epub/toc_parser.cpp
    src/epub/reading_position.cpp
    src/render/litehtml_container.cpp
    src/render/page_renderer.cpp
    src/utils/logger.cpp
)

set(HEADERS
    src/epub/epub_parser.h
    src/epub/zip_handler.h
    src/epub/image_cache.h
    src/epub/toc_parser.h
    src/epub/reading_position.h
    src/render/litehtml_container.h
    src/render/page_renderer.h
    src/utils/logger.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE app.manifest)
endif()

target_link_libraries(${PROJECT_NAME} 
    litehtml
    gumbo
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} 
        comctl32
        gdi32
        gdiplus
        msimg32
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/test.epub")
    configure_file(
        "${CMAKE_SOURCE_DIR}/test.epub"
        "${CMAKE_BINARY_DIR}/test.epub"
        COPYONLY
    )
endif()