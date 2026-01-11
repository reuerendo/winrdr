cmake_minimum_required(VERSION 3.15)
project(EpubReader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DLXB_STATIC -DLEXBOR_STATIC -DLXB_API=)

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    add_compile_options(/utf-8)
endif()

set(LEXBOR_DIR "${CMAKE_SOURCE_DIR}/libs/lexbor")
set(LEXBOR_INCLUDE "${LEXBOR_DIR}/source")

message(STATUS "========================================")
message(STATUS "Searching for Lexbor libraries")
message(STATUS "========================================")

file(GLOB_RECURSE ALL_LIBS 
    "${LEXBOR_DIR}/build/*.lib" 
    "${LEXBOR_DIR}/build/*.a"
)

message(STATUS "Found library files:")
foreach(lib ${ALL_LIBS})
    message(STATUS "  ${lib}")
endforeach()

set(LEXBOR_LIBRARIES "")

foreach(lib ${ALL_LIBS})
    get_filename_component(lib_name ${lib} NAME_WE)
    if(lib_name MATCHES "lexbor")
        list(APPEND LEXBOR_LIBRARIES ${lib})
    endif()
endforeach()

if(NOT LEXBOR_LIBRARIES)
    message(FATAL_ERROR 
        "No Lexbor libraries found!\n"
        "Please build lexbor first:\n"
        "  cd libs/lexbor\n"
        "  cmake -B build -DCMAKE_BUILD_TYPE=Release -DLEXBOR_BUILD_SHARED=OFF -DLEXBOR_BUILD_STATIC=ON\n"
        "  cmake --build build --config Release"
    )
endif()

message(STATUS "Libraries to link:")
foreach(lib ${LEXBOR_LIBRARIES})
    message(STATUS "  ${lib}")
endforeach()
message(STATUS "========================================")

set(SOURCES
    src/main.cpp
    src/epub/epub_parser.cpp
    src/epub/zip_handler.cpp
    src/epub/image_cache.cpp
    src/epub/html_processor.cpp
    src/epub/css_processor.cpp
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
    src/epub/css_processor.h
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

message(STATUS "Linking libraries to target:")
foreach(lib ${LEXBOR_LIBRARIES})
    message(STATUS "  ${lib}")
    target_link_libraries(${PROJECT_NAME} ${lib})
endforeach()

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

if(EXISTS "${CMAKE_SOURCE_DIR}/epub.css")
    configure_file(
        "${CMAKE_SOURCE_DIR}/epub.css"
        "${CMAKE_BINARY_DIR}/Release/epub.css"
        COPYONLY
    )
    configure_file(
        "${CMAKE_SOURCE_DIR}/epub.css"
        "${CMAKE_BINARY_DIR}/Debug/epub.css"
        COPYONLY
    )
endif()