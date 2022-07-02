# -*- cmake -*-
if (BOOST_CMAKE_INCLUDED)
	return()
endif (BOOST_CMAKE_INCLUDED)
set (BOOST_CMAKE_INCLUDED TRUE)

include(Prebuilt)
include(Variables)

if (ARCH STREQUAL "x86_64")
	set(ARCH_SUFFIX "-x64")
elseif (ARCH STREQUAL "arm64")
	set(ARCH_SUFFIX "-a64")
endif ()

# Disable system libs support for now, since our boost fiber library is
# patched, which is not the case on a standard system.
#if (USESYSTEMLIBS)
#  set(Boost_FIND_QUIETLY OFF)
#  set(Boost_FIND_REQUIRED ON)
#  include(FindBoost)
#
#  set(BOOST_ATOMIC_LIBRARY boost_atomic-mt)
#  set(BOOST_CONTEXT_LIBRARY boost_context-mt)
#  set(BOOST_FIBER_LIBRARY boost_fiber-mt)
#  set(BOOST_PROGRAM_OPTIONS_LIBRARY boost_program_options-mt)
#  set(BOOST_FILESYSTEM_LIBRARY boost_filesystem-mt)
#  set(BOOST_THREAD_LIBRARY boost_thread-mt)
#else (USESYSTEMLIBS)
	set(Boost_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)

	if (DARWIN)
		use_prebuilt_binary(boost)

		set(BOOST_ATOMIC_LIBRARY boost_atomic-mt${ARCH_SUFFIX})
		set(BOOST_CONTEXT_LIBRARY boost_context-mt${ARCH_SUFFIX})
		set(BOOST_FIBER_LIBRARY boost_fiber-mt${ARCH_SUFFIX})
		set(BOOST_FILESYSTEM_LIBRARY boost_filesystem-mt${ARCH_SUFFIX})
		set(BOOST_PROGRAM_OPTIONS_LIBRARY boost_program_options-mt${ARCH_SUFFIX})
		set(BOOST_THREAD_LIBRARY boost_thread-mt${ARCH_SUFFIX})
	elseif (LINUX)
		if (${CXX_VERSION} GREATER 749 AND NOT (${CXX_VERSION} GREATER 759))
			use_prebuilt_binary(boost-static-gcc75)
		else ()
			# Given how screwed are symbols in static boost libraries, we
			# cannot link them properly with objects compiled with a different
			# gcc version than the version used to build boost itself (v7.5),
			# so let's use their shared libraries counterparts in that case (at
			# the cost of a larger distribution package)...
			use_prebuilt_binary(boost)
		endif ()

		set(BOOST_ATOMIC_LIBRARY
			optimized boost_atomic-mt${ARCH_SUFFIX}
			debug boost_atomic-mt-d${ARCH_SUFFIX})
		set(BOOST_CONTEXT_LIBRARY
			optimized boost_context-mt${ARCH_SUFFIX}
			debug boost_context-mt-d${ARCH_SUFFIX})
		set(BOOST_FIBER_LIBRARY
			optimized boost_fiber-mt${ARCH_SUFFIX}
			debug boost_fiber-mt-d${ARCH_SUFFIX})
		set(BOOST_FILESYSTEM_LIBRARY
			optimized boost_filesystem-mt${ARCH_SUFFIX}
			debug boost_filesystem-mt-d${ARCH_SUFFIX})
		set(BOOST_PROGRAM_OPTIONS_LIBRARY
			optimized boost_program_options-mt${ARCH_SUFFIX}
			debug boost_program_options-mt-d${ARCH_SUFFIX})
		set(BOOST_THREAD_LIBRARY
			optimized boost_thread-mt${ARCH_SUFFIX}
			debug boost_thread-mt-d${ARCH_SUFFIX})
	elseif (WINDOWS)
		use_prebuilt_binary(boost)

		# Do not let Visual Studio try and auto-link boost libraries !
		add_definitions(-DBOOST_ALL_NO_LIB)

		set(BOOST_ATOMIC_LIBRARY libboost_atomic-mt${ARCH_SUFFIX})
		set(BOOST_CONTEXT_LIBRARY libboost_context-mt${ARCH_SUFFIX})
		set(BOOST_FIBER_LIBRARY libboost_fiber-mt${ARCH_SUFFIX})
		set(BOOST_FILESYSTEM_LIBRARY libboost_filesystem-mt${ARCH_SUFFIX})
		set(BOOST_PROGRAM_OPTIONS_LIBRARY libboost_program_options-mt${ARCH_SUFFIX})
		set(BOOST_THREAD_LIBRARY libboost_thread-mt${ARCH_SUFFIX})
  endif ()
#endif (USESYSTEMLIBS)
