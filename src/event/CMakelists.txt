set(HEADER_FILES 
		Event.h
		ApplicationEvent.h
		KeyboardEvent.h
		MouseEvent.h		
	)

add_library(gbcemu_event
		${HEADER_FILES}
	)

set_target_properties(gbcemu_event PROPERTIES LINKER_LANGUAGE CXX)

set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_compile_options(gbcemu_event PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_include_directories(gbcemu_event PUBLIC ${SRC_PATH})

target_link_libraries(gbcemu_event PRIVATE gbcemu_util)
target_link_libraries(gbcemu_event PRIVATE gbcemu_common)
