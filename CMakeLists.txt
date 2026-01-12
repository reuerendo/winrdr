cmake_minimum_required(VERSION 3.15)
project(EpubReader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
    add_compile_options(/utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

set(LITEHTML_UTF8 ON CACHE BOOL "Force UTF-8 strings in litehtml" FORCE)
set(LITEHTML_BUILD_TESTING OFF CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_SOURCE_DIR}/libs/litehtml/CMakeLists.txt")
    add_subdirectory(libs/litehtml EXCLUDE_FROM_ALL)
    
    target_compile_definitions(litehtml PUBLIC LITEHTML_UTF8)
else()
    message(FATAL_ERROR "litehtml not found in libs/litehtml. Please check build.yml")
endif()

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

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    libs/litehtml/include
)

target_link_libraries(${PROJECT_NAME} 
    litehtml
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} 
        comctl32
        gdi32
        gdiplus
        msimg32
        shlwapi
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/test.epub")
    configure_file(
        "${CMAKE_SOURCE_DIR}/test.epub"
        "${CMAKE_BINARY_DIR}/test.epub"
        COPYONLY
    )
endif()