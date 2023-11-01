# -*- cmake -*-
include(Prebuilt)

add_library( ll::glh-linear INTERFACE IMPORTED )


use_prebuilt_binary(glh-linear)