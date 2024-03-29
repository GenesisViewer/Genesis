# -*- cmake -*-

project(llprimitive)

include(00-Common)
include(LLCommon)
include(LLMath)
include(LLMessage)
include(LLXML)
include(LLPhysicsExtensions)
include(Colladadom)
include(LLCharacter)
include(GLH)
include(TinyGLTF)

include_directories(
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLMESSAGE_INCLUDE_DIRS}
    ${LLXML_INCLUDE_DIRS}
    ${COLLADADOM_INCLUDE_DIRS}
    ${LLCHARACTER_INCLUDE_DIRS}
    )
include_directories(SYSTEM
    ${LLCOMMON_SYSTEM_INCLUDE_DIRS}
    ${LLXML_SYSTEM_INCLUDE_DIRS}
    ${LLPHYSICSEXTENSIONS_INCLUDE_DIRS}
    )

set(llprimitive_SOURCE_FILES
    lldaeloader.cpp
    llgltfloader.cpp
    llgltfmaterial.cpp
    llmaterialid.cpp
    llmaterial.cpp
    llmaterialtable.cpp
    llmediaentry.cpp
    llmodel.cpp
    llmodelloader.cpp
    llprimitive.cpp
    llprimtexturelist.cpp
    lltextureanim.cpp
    lltextureentry.cpp
    lltreeparams.cpp
    llvolumemessage.cpp
    llvolumexml.cpp
    material_codes.cpp
    )

set(llprimitive_HEADER_FILES
    CMakeLists.txt
    lldaeloader.h
    llgltfloader.h
    legacy_object_types.h
    llgltfmaterial.h
    llmaterial.h
    llmaterialid.h
    llmaterialtable.h
    llmediaentry.h
    llmodel.h
    llmodelloader.h
    llprimitive.h
    llprimtexturelist.h
    lltextureanim.h
    lltextureentry.h
    lltree_common.h
    lltreeparams.h
    llvolumemessage.h
    llvolumexml.h
    material_codes.h
    object_flags.h
    )
        
set_source_files_properties(${llprimitive_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llprimitive_SOURCE_FILES ${llprimitive_HEADER_FILES})

add_library (llprimitive ${llprimitive_SOURCE_FILES})

target_link_libraries(
    llprimitive
    
    PUBLIC
    llcommon
    )
