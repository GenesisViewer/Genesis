# -*- cmake -*-

project(genxsqlite)
include(00-Common)
include(Boost)
include(LLCommon)
include_directories(${LIBS_OPEN_DIR}/llcommon)
include_directories(
    ${LLCOMMON_INCLUDE_DIRS}
    )
set(genxsqlite_SOURCE_FILES
     sqlite3.c
    llsqlmgr.cpp
    )

set(genxsqlite_HEADER_FILES
    CMakeLists.txt

    sqlite3.h
    llsqlmgr.h
    )

set_source_files_properties(${genxsqlite_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND genxsqlite_SOURCE_FILES ${genxsqlite_HEADER_FILES})

add_library (genxsqlite ${genxsqlite_SOURCE_FILES})

target_link_libraries(
    genxsqlite
    PUBLIC
    )
