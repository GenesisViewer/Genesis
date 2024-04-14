# -*- cmake -*-
#include(Variables)
#include(Prebuilt)
#use_prebuilt_binary(webrtc)

#set(llwebrtc_INCLUDE_DIR
#    ${LIBS_OPEN_DIR}/llwebrtc
#    ${LIBS_PREBUILT_DIR}/include/webrtc
#    )

#set(WEBRTC_LIBRARIES webrtc)

set(llwebrtc_SOURCE_FILES
    llwebrtc.cpp
    )

set(llwebrtc_HEADER_FILES
    CMakeLists.txt
    llwebrtc.h
    llwebrtc_impl.h
    )
    include_directories(
        ${LLCOMMON_INCLUDE_DIRS}
        ${LIBS_PREBUILT_DIR}/include/webrtc
        ${LIBS_PREBUILT_DIR}/include/webrtc/third_party/abseil-cpp
        )
list(APPEND llwebrtc_SOURCE_FILES ${llwebrtc_HEADER_FILES})