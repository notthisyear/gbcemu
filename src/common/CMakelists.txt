set(HEADER_FILES 
		KeyCode.h
		MouseCode.h
		WindowProperties.h
	)

set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_library(gbcemu_common
		${HEADER_FILES}
	)

set_target_properties(gbcemu_common PROPERTIES LINKER_LANGUAGE CXX)

target_compile_options(gbcemu_common PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_include_directories(gbcemu_common PUBLIC ${SRC_PATH})

target_link_libraries(gbcemu_common PRIVATE gbcemu_util)
