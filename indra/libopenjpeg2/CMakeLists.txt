# -*- cmake -*-

project(openjpeg)

include(00-Common)


# OPENJPEG version number, useful for packaging and doxygen doc:
set(OPENJPEG_VERSION_MAJOR 2)
set(OPENJPEG_VERSION_MINOR 5)
set(OPENJPEG_VERSION_BUILD 2)
set(OPENJPEG_VERSION
  "${OPENJPEG_VERSION_MAJOR}.${OPENJPEG_VERSION_MINOR}.${OPENJPEG_VERSION_BUILD}")
add_definitions(-DUSE_JPIP)
add_definitions(-DMUTEX_win32)
set(openjpeg_SOURCE_FILES
    bio.c
    cidx_manager.c
    cio.c
    dwt.c
    event.c
    function_list.c
    ht_dec.c
    image.c
    invert.c
    j2k.c
    jp2.c
    
    mct.c
    mqc.c
    openjpeg.c
    opj_clock.c
    opj_malloc.c
    phix_manager.c
    pi.c
    ppix_manager.c
    sparse_array.c
    t1_generate_luts.c
    t1_ht_generate_luts.c
    t1.c
    t2.c
    tcd.c
    tgt.c
    thix_manager.c
    thread.c
    tpix_manager.c
)

set(openjpeg_HEADER_FILES
    bio.h
    cidx_manager.h
    cio.h
    dwt.h
    event.h
    function_list.h
    image.h
    indexbox_manager.h
    invert.h
    
    j2k.h
    
    jp2.h
    
    mct.h
    mqc_inl.h
    mqc.h
    openjpeg.h
    opj_clock.h
    opj_codec.h
    opj_common.h
    opj_includes.h
    opj_intmath.h
    opj_malloc.h
    pi.h
    sparse_array.h
    t1.h
    t1_luts.h
    t1_ht_luts.h
    t2.h
    tcd.h
    tgt.h
    tls_keys.h
)

IF(WINDOWS)
   add_definitions(-D_CRT_SECURE_NO_WARNINGS)
   add_definitions(-DOPJ_STATIC)
ENDIF(WINDOWS)


set_source_files_properties(${openjpeg_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND openjpeg_SOURCE_FILES ${openjpeg_HEADER_FILES})

add_library (openjpeg ${openjpeg_SOURCE_FILES})

# This setting of SOVERSION assumes that any API change
# will increment either the minor or major version number of openjpeg
set(OPENJPEG_LIBRARY_PROPERTIES
  VERSION   "${OPENJPEG_VERSION_MAJOR}.${OPENJPEG_VERSION_MINOR}.${OPENJPEG_VERSION_BUILD}"
  SOVERSION "${OPENJPEG_VERSION_MAJOR}.${OPENJPEG_VERSION_MINOR}"
)

set_target_properties(openjpeg PROPERTIES
	${OPENJPEG_LIBRARY_PROPERTIES})

