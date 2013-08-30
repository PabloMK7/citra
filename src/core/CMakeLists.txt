set(SRCS	src/core.cpp
			src/memory.cpp
			src/boot/apploader.cpp
			src/boot/bootrom.cpp
            src/boot/loader.cpp
			src/hle/hle.cpp
			src/hw/hw.cpp
	    )

add_library(core STATIC ${SRCS})
