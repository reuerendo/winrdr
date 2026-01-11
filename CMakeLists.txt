cmake_minimum_required(VERSION 3.15)
project(EpubReader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    add_compile_options(/utf-8)
endif()

set(LEXBOR_DIR "${CMAKE_SOURCE_DIR}/libs/lexbor")
set(LEXBOR_INCLUDE "${LEXBOR_DIR}/source")

function(find_lexbor_library output_var lib_name)
    set(search_paths
        "${LEXBOR_DIR}/build/${lib_name}.lib"
        "${LEXBOR_DIR}/build/Release/${lib_name}.lib"
        "${LEXBOR_DIR}/build/Debug/${lib_name}.lib"
        "${LEXBOR_DIR}/build/${lib_name}.a"
        "${LEXBOR_DIR}/build/source/lexbor/core/Release/${lib_name}.lib"
        "${LEXBOR_DIR}/build/source/lexbor/core/Debug/${lib_name}.lib"
        "${LEXBOR_DIR}/build/source/lexbor/html/Release/${lib_name}.lib"
        "${LEXBOR_DIR}/build/source/lexbor/html/Debug/${lib_name}.lib"
        "${LEXBOR_DIR}/build/source/lexbor/dom/Release/${lib_name}.lib"
        "${LEXBOR_DIR}/build/source/lexbor/dom/Debug/${lib_name}.lib"
    )
    
    foreach(lib_path ${search_paths})
        if(EXISTS "${lib_path}")
            set(${output_var} "${lib_path}" PARENT_SCOPE)
            message(STATUS "Found ${lib_name}: ${lib_path}")
            return()
        endif()
    endforeach()
    
    file(GLOB_RECURSE found_lib "${LEXBOR_DIR}/build/**/${lib_name}.lib" "${LEXBOR_DIR}/build/**/${lib_name}.a")
    if(found_lib)
        list(GET found_lib 0 first_lib)
        set(${output_var} "${first_lib}" PARENT_SCOPE)
        message(STATUS "Found ${lib_name} via search: ${first_lib}")
    else()
        message(WARNING "${lib_name} not found")
        set(${output_var} "" PARENT_SCOPE)
    endif()
endfunction()

find_lexbor_library(LEXBOR_CORE_LIB "lexbor_core_static")
find_lexbor_library(LEXBOR_DOM_LIB "lexbor_dom_static")
find_lexbor_library(LEXBOR_HTML_LIB "lexbor_html_static")
find_lexbor_library(LEXBOR_TAG_LIB "lexbor_tag_static")
find_lexbor_library(LEXBOR_NS_LIB "lexbor_ns_static")

set(LEXBOR_LIBRARIES "")
if(LEXBOR_CORE_LIB)
    list(APPEND LEXBOR_LIBRARIES ${LEXBOR_CORE_LIB})
endif()
if(LEXBOR_DOM_LIB)
    list(APPEND LEXBOR_LIBRARIES ${LEXBOR_DOM_LIB})
endif()
if(LEXBOR_HTML_LIB)
    list(APPEND LEXBOR_LIBRARIES ${LEXBOR_HTML_LIB})
endif()
if(LEXBOR_TAG_LIB)
    list(APPEND LEXBOR_LIBRARIES ${LEXBOR_TAG_LIB})
endif()
if(LEXBOR_NS_LIB)
    list(APPEND LEXBOR_LIBRARIES ${LEXBOR_NS_LIB})
endif()

if(NOT LEXBOR_LIBRARIES)
    file(GLOB_RECURSE ALL_LEXBOR_LIBS "${LEXBOR_DIR}/build/*.lib" "${LEXBOR_DIR}/build/*.a")
    
    if(ALL_LEXBOR_LIBS)
        message(STATUS "Using all found Lexbor libraries:")
        foreach(lib ${ALL_LEXBOR_LIBS})
            message(STATUS "  ${lib}")
            list(APPEND LEXBOR_LIBRARIES ${lib})
        endforeach()
    else()
        message(FATAL_ERROR 
            "Lexbor libraries not found. Please build lexbor first:\n"
            "  cd libs/lexbor\n"
            "  cmake -B build -DCMAKE_BUILD_TYPE=Release -DLEXBOR_BUILD_SHARED=OFF -DLEXBOR_BUILD_STATIC=ON\n"
            "  cmake --build build --config Release"
        )
    endif()
endif()

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
target_link_libraries(${PROJECT_NAME} ${LEXBOR_LIBRARIES})

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