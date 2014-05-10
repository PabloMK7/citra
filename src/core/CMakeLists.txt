set(SRCS    core.cpp
            core_timing.cpp
            loader.cpp
            mem_map.cpp
            mem_map_funcs.cpp
            system.cpp
            arm/disassembler/arm_disasm.cpp
            arm/disassembler/load_symbol_map.cpp
            arm/interpreter/arm_interpreter.cpp
            arm/interpreter/armemu.cpp
            arm/interpreter/arminit.cpp
            arm/interpreter/armmmu.cpp
            arm/interpreter/armos.cpp
            arm/interpreter/armsupp.cpp
            arm/interpreter/armvirt.cpp
            arm/interpreter/thumbemu.cpp
            arm/mmu/arm1176jzf_s_mmu.cpp
            elf/elf_reader.cpp
            file_sys/directory_file_system.cpp
            file_sys/meta_file_system.cpp
            hle/hle.cpp
            hle/config_mem.cpp
            hle/coprocessor.cpp
            hle/syscall.cpp
            hle/kernel/kernel.cpp
            hle/kernel/thread.cpp
            hle/service/apt.cpp
            hle/service/gsp.cpp
            hle/service/hid.cpp
            hle/service/service.cpp
            hle/service/srv.cpp
            hw/hw.cpp
            hw/lcd.cpp
            hw/ndma.cpp)

add_library(core STATIC ${SRCS})
