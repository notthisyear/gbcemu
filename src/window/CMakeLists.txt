set(HEADER_FILES 
		Window.h
	)

set(SOURCE_FILES 
		Window.cpp
	)

set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(EXT_PATH ${SRC_PATH}/../external)

add_library(gbcemu_window
		${SOURCE_FILES}
		${HEADER_FILES}
	)

target_include_directories(gbcemu_window PUBLIC ${SRC_PATH})
target_include_directories(gbcemu_window PUBLIC ${EXT_PATH}/glfw/include)

target_compile_options(gbcemu_window PRIVATE -Wall -Wextra -Wpedantic -Werror)

target_link_libraries(gbcemu_window PRIVATE gbcemu_opengl)
target_link_libraries(gbcemu_window PRIVATE gbcemu_event)
target_link_libraries(gbcemu_window PRIVATE glfw)
target_link_libraries(gbcemu_window PRIVATE gbcemu_common)