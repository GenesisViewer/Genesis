# -*- cmake -*-

project(genxcuda)
include(00-Common)
include(Boost)
include(CUDA)
include(LLCommon)
include_directories(${LIBS_OPEN_DIR}/llcommon)
include_directories(
    ${LLCOMMON_INCLUDE_DIRS}
    )

set(genxcuda_SOURCE_FILES
     genxcudamgr.cpp
    )

set(genxcuda_HEADER_FILES
    CMakeLists.txt

    genxcudamgr.h
    )

set_source_files_properties(${genxcuda_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND genxcuda_SOURCE_FILES ${genxcuda_HEADER_FILES})

add_library (genxcuda ${genxcuda_SOURCE_FILES})

target_link_libraries(
    genxcuda
    PUBLIC
    )
