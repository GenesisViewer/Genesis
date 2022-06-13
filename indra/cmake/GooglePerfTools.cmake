# -*- cmake -*-

include(Prebuilt)

if(ADDRESS_SIZE EQUAL 64)
  set(DISABLE_TCMALLOC TRUE)
endif(ADDRESS_SIZE EQUAL 64)

if (STANDALONE)
  include(FindGooglePerfTools)
else (STANDALONE)
  if (LINUX OR WINDOWS AND NOT ADDRESS_SIZE EQUAL 64)
    use_prebuilt_binary(gperftools)
  endif (LINUX OR WINDOWS AND NOT ADDRESS_SIZE EQUAL 64)
  if (WINDOWS AND NOT DISABLE_TCMALLOC)
    set(TCMALLOC_LIBRARIES libtcmalloc_minimal.lib)
    set(TCMALLOC_LINKER_FLAGS "/INCLUDE:\"__tcmalloc\"")
  endif (WINDOWS AND NOT DISABLE_TCMALLOC)
  if (LINUX)
    if(USE_GOOGLE_PERFTOOLS)
      set(TCMALLOC_LIBRARIES tcmalloc)
      set(STACKTRACE_LIBRARIES stacktrace)
      set(PROFILER_LIBRARIES profiler)
    else()
      set(TCMALLOC_LIBRARIES tcmalloc_minimal)
    endif()
    set(GOOGLE_PERFTOOLS_INCLUDE_DIR
      ${LIBS_PREBUILT_DIR}/include
      ${LIBS_PREBUILT_LEGACY_DIR}/include
      )
  endif (LINUX)
endif (STANDALONE)

if (GOOGLE_PERFTOOLS_FOUND AND STANDALONE)
  set(USE_GOOGLE_PERFTOOLS ON CACHE BOOL "Build with Google PerfTools support.")
else ()
  set(USE_GOOGLE_PERFTOOLS OFF)
endif ()

# XXX Disable temporarily, until we have compilation issues on 64-bit
# Etch sorted.
#set(USE_GOOGLE_PERFTOOLS OFF)

if (USE_GOOGLE_PERFTOOLS)
  set(TCMALLOC_FLAG -DLL_USE_TCMALLOC=1)
  include_directories(${GOOGLE_PERFTOOLS_INCLUDE_DIR})
  set(GOOGLE_PERFTOOLS_LIBRARIES ${TCMALLOC_LIBRARIES} ${STACKTRACE_LIBRARIES} ${PROFILER_LIBRARIES})
else (USE_GOOGLE_PERFTOOLS)
  set(TCMALLOC_FLAG -ULL_USE_TCMALLOC)
endif (USE_GOOGLE_PERFTOOLS)

if (NOT(DISABLE_TCMALLOC OR USE_GOOGLE_PERFTOOLS OR STANDALONE))
  if (NOT STATUS_Building_with_Google_TCMalloc)
    message(STATUS "Building with Google TCMalloc")
	set(STATUS_Building_with_Google_TCMalloc true PARENT_SCOPE)
  endif (NOT STATUS_Building_with_Google_TCMalloc)
  set(TCMALLOC_FLAG -DLL_USE_TCMALLOC=1)
  include_directories(${GOOGLE_PERFTOOLS_INCLUDE_DIR})
  set(GOOGLE_PERFTOOLS_LIBRARIES ${TCMALLOC_LIBRARIES})
  set(GOOGLE_PERFTOOLS_LINKER_FLAGS ${TCMALLOC_LINKER_FLAGS})
endif()

add_definitions(${TCMALLOC_FLAG})