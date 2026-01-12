cmake_minimum_required(VERSION 3.15)
project(EpubReader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    add_compile_options(/utf-8)
endif()

# Gumbo-parser
set(GUMBO_SOURCES
    libs/gumbo-parser/src/attribute.c
    libs/gumbo-parser/src/char_ref.c
    libs/gumbo-parser/src/error.c
    libs/gumbo-parser/src/parser.c
    libs/gumbo-parser/src/string_buffer.c
    libs/gumbo-parser/src/string_piece.c
    libs/gumbo-parser/src/tag.c
    libs/gumbo-parser/src/tokenizer.c
    libs/gumbo-parser/src/utf8.c
    libs/gumbo-parser/src/util.c
    libs/gumbo-parser/src/vector.c
)

add_library(gumbo STATIC ${GUMBO_SOURCES})
target_include_directories(gumbo PUBLIC libs/gumbo-parser/src)

# Litehtml
set(LITEHTML_SOURCES
    libs/litehtml/src/background.cpp
    libs/litehtml/src/box.cpp
    libs/litehtml/src/context.cpp
    libs/litehtml/src/css_length.cpp
    libs/litehtml/src/css_selector.cpp
    libs/litehtml/src/document.cpp
    libs/litehtml/src/el_anchor.cpp
    libs/litehtml/src/el_base.cpp
    libs/litehtml/src/el_before_after.cpp
    libs/litehtml/src/el_body.cpp
    libs/litehtml/src/el_break.cpp
    libs/litehtml/src/el_cdata.cpp
    libs/litehtml/src/el_comment.cpp
    libs/litehtml/src/el_div.cpp
    libs/litehtml/src/el_font.cpp
    libs/litehtml/src/el_image.cpp
    libs/litehtml/src/el_link.cpp
    libs/litehtml/src/el_para.cpp
    libs/litehtml/src/el_script.cpp
    libs/litehtml/src/el_space.cpp
    libs/litehtml/src/el_style.cpp
    libs/litehtml/src/el_table.cpp
    libs/litehtml/src/el_td.cpp
    libs/litehtml/src/el_text.cpp
    libs/litehtml/src/el_title.cpp
    libs/litehtml/src/el_tr.cpp
    libs/litehtml/src/element.cpp
    libs/litehtml/src/html.cpp
    libs/litehtml/src/html_tag.cpp
    libs/litehtml/src/iterators.cpp
    libs/litehtml/src/media_query.cpp
    libs/litehtml/src/style.cpp
    libs/litehtml/src/stylesheet.cpp
    libs/litehtml/src/table.cpp
    libs/litehtml/src/utf8_strings.cpp
    libs/litehtml/src/web_color.cpp
)

add_library(litehtml STATIC ${LITEHTML_SOURCES})
target_include_directories(litehtml PUBLIC libs/litehtml/include)
target_link_libraries(litehtml PUBLIC gumbo)

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