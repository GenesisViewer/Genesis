# -*- cmake -*-

# some webrtc headers require C++ 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(00-Common)
include(Variables)
include (Prebuilt)
include(WebRTC)
use_prebuilt_binary(webrtc)
project(llwebrtc)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")



add_library( llwebrtc ${llwebrtc_SOURCE_FILES} )
#"${LIBS_PREBUILT_DIR}/include/webrtc/third_party/abseil-cpp"
target_include_directories( llwebrtc PUBLIC INTERFACE "${LIBS_PREBUILT_DIR}/include/webrtc" )
    
target_link_libraries( llwebrtc PUBLIC ${LIBS_PREBUILT_DIR}/lib/release/webrtc.lib )
#ADD_CUSTOM_COMMAND(OUTPUT ${SHARED_LIB_STAGING_DIR}
  #                 COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       
  #                             )

#set_target_properties(llwebrtc PROPERTIES PUBLIC_HEADER llwebrtc.h)

