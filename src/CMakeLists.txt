set(EXT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../external)

set(HEADER_FILES
		Application.h
	)

set(SOURCE_FILES 
		gbcemu.cpp
		Application.cpp
	)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# External
add_subdirectory(${EXT_PATH}/glad ${CMAKE_BINARY_DIR}/external/glad)
add_subdirectory(${EXT_PATH}/glfw ${CMAKE_BINARY_DIR}/external/glfw)

# Internal
add_subdirectory(util)
add_subdirectory(debugger)
add_subdirectory(common)
add_subdirectory(event)
add_subdirectory(components)
add_subdirectory(opengl)
add_subdirectory(window)

# Target
add_executable(gbcemu_main ${HEADER_FILES} ${SOURCE_FILES})

# Subdirectories
target_include_directories(gbcemu_main PUBLIC ${EXT_PATH})
target_include_directories(gbcemu_main PUBLIC ${EXT_PATH}/glad/include)

target_compile_options(gbcemu_main PRIVATE -Wall -Wextra -Wpedantic -Werror)

# Libraries
target_link_libraries(gbcemu_main PRIVATE gbcemu_util)
target_link_libraries(gbcemu_main PRIVATE gbcemu_debugger)
target_link_libraries(gbcemu_main PRIVATE gbcemu_components)
target_link_libraries(gbcemu_main PRIVATE gbcemu_opengl)
target_link_libraries(gbcemu_main PRIVATE gbcemu_window)