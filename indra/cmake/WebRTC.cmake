# -*- cmake -*-
include(Variables)
include(Prebuilt)
use_prebuilt_binary(webrtc)
add_library( ll::webrtc INTERFACE IMPORTED )
target_include_directories( ll::webrtc SYSTEM INTERFACE "${LIBS_PREBUILT_DIR}/include/webrtc" "${LIBS_PREBUILT_DIR}/include/webrtc/third_party/abseil-cpp")

set(llwebrtc_INCLUDE_DIR
    ${LIBS_OPEN_DIR}/llwebrtc
    ${LIBS_PREBUILT_DIR}/include/webrtc
    )

set(WEBRTC_LIBRARIES webrtc)
target_link_libraries( ll::webrtc INTERFACE webrtc.lib )