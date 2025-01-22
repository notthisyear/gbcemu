set(HEADER_FILES
		Opcodes.h
		OpcodeBuilder.h
		MMU.h
		CPU.h
        Cartridge.h
		PPU.h
		PixelFetcher.h
		TimerController.h
	)

set(SOURCE_FILES 
		CPU.cpp
		MMU.cpp
		Cartridge.cpp
		PPU.cpp
		PixelFetcher.cpp
		TimerController.cpp
	)

set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_library(gbcemu_components
		${SOURCE_FILES}
		${HEADER_FILES}
	)

target_compile_options(gbcemu_components PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_include_directories(gbcemu_components PUBLIC ${SRC_PATH})


target_link_libraries(gbcemu_components PRIVATE gbcemu_util)