cmake_minimum_required(VERSION 3.15)
project(EpubReader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    add_compile_options(/utf-8)
endif()

# Lexbor library
set(LEXBOR_DIR "${CMAKE_SOURCE_DIR}/libs/lexbor")
if(EXISTS "${LEXBOR_DIR}/build/Release/liblexbor_static.lib")
    set(LEXBOR_LIBRARY "${LEXBOR_DIR}/build/Release/liblexbor_static.lib")
elseif(EXISTS "${LEXBOR_DIR}/build/liblexbor_static.a")
    set(LEXBOR_LIBRARY "${LEXBOR_DIR}/build/liblexbor_static.a")
else()
    message(FATAL_ERROR "Lexbor library not found. Please build lexbor first.")
endif()

set(LEXBOR_INCLUDE "${LEXBOR_DIR}/source")

set(SOURCES
    src/main.cpp
    src/epub/epub_parser.cpp
    src/epub/zip_handler.cpp
    src/epub/image_cache.cpp
    src/epub/html_processor.cpp
    src/epub/toc_parser.cpp
    src/epub/reading_position.cpp
    src/render/page_renderer.cpp
    src/utils/logger.cpp
)

set(HEADERS
    src/epub/epub_parser.h
    src/epub/zip_handler.h
    src/epub/image_cache.h
    src/epub/formatted_text.h
    src/epub/html_processor.h
    src/epub/toc_parser.h
    src/epub/reading_position.h
    src/render/page_renderer.h
    src/utils/logger.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE app.manifest)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${LEXBOR_INCLUDE})
target_link_libraries(${PROJECT_NAME} ${LEXBOR_LIBRARY})

if(WIN32)
    target_link_libraries(${PROJECT_NAME} 
        comctl32
        gdi32
        gdiplus
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/test.epub")
    configure_file(
        "${CMAKE_SOURCE_DIR}/test.epub"
        "${CMAKE_BINARY_DIR}/test.epub"
        COPYONLY
    )
endif()