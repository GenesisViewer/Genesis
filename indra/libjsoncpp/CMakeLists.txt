# -*- cmake -*-

project(jsoncpp)

include(00-Common)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/json)

set(jsoncpp_SOURCE_FILES
	json_reader.cpp
	json_value.cpp
	json_writer.cpp
)

set(jsoncpp_HEADER_FILES
    json_valueiterator.inl
	json_tool.h
	json/json_config.h
	json/json_features.h
	json/forwards.h
	json/value.h
	json/reader.h
	json/writer.h
	json/assertions.h
	json/version.h
)

set_source_files_properties(${jsoncpp_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND jsoncpp_SOURCE_FILES ${jsoncpp_HEADER_FILES})

add_library(jsoncpp ${jsoncpp_SOURCE_FILES})
# We do not need locale support...
add_definitions(-DJSONCPP_NO_LOCALE_SUPPORT)

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  if (USEUNITYBUILD)
    set_target_properties(jsoncpp PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 0)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("libjsoncpp COMPILE_DEFINITIONS = ${ALLDEFINES}")
