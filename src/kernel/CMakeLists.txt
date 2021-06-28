cmake_minimum_required(VERSION 3.14)

project(kernel VERSION 0.1 DESCRIPTION "Twizzler OS -- Kernel")

enable_language(C ASM)

set(TWZ_SERIAL_DEBUG_BAUD "38400" CACHE STRING "Kernel serial debug console baud rate")
set(TWZ_SERIAL_DEBUG_STOPBITS "1" CACHE STRING "Kernel serial debug console stop bits")
set(TWZ_SERIAL_DEBUG_WORDSZ "8" CACHE STRING "Kernel serial debug console word size")
option(TWZ_KERNEL_UBSAN "Enable UBSAN in the kernel")

add_compile_options("-g")

add_compile_options("-DCONFIG_SERIAL_DEBUG_BAUD=${TWZ_SERIAL_DEBUG_BAUD}")
add_compile_options("-DCONFIG_SERIAL_DEBUG_STOPBITS=${TWZ_SERIAL_DEBUG_STOPBITS}")
add_compile_options("-DCONFIG_SERIAL_DEBUG_WORDSZ=${TWZ_SERIAL_DEBUG_WORDSZ}")

if("${TWZ_TARGET}" STREQUAL "x86_64")
	add_compile_options("-DFEATURE_SUPPORTED_UNWIND")
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	add_compile_options("-DCONFIG_DEBUG_LOCKS=1")
	add_compile_options("-DCONFIG_DEBUG=1")
endif()

#if(TWZ_KERNEL_UBSAN)
	add_compile_options("-fsanitize=undefined" "-DCONFIG_UBSAN")
	#endif()

add_compile_options("-DCONFIG_ARCH=${TWIZZLER_PROCESSOR}")
add_compile_options("-DCONFIG_MACHINE=${TWIZZLER_MACHINE}")

# TODO: remove this
add_compile_options("-Wno-address-of-packed-member")
add_compile_options("-Wno-atomic-alignment")


list(APPEND KERNEL_SOURCES
	core/clksrc.c
	core/csprng.c
	core/debug.c
	core/interrupt.c
	core/kc.c
	core/ksymbol.c
	core/ksymbol_weak.c
	core/main.c
	core/nvdimm.c
	core/panic.c
	core/processor.c
	core/rand.c
	core/rwlock.c
	core/schedule.c
	core/spinlock.c
	core/syscall.c
	core/thread.c
	core/timer.c
	core/ubsan.c
	core/unwind.c
	core/mm/kalloc.c
	core/mm/kheap.c
	core/mm/map.c
	core/mm/memory.c
	core/mm/objspace.c
	core/mm/page.c
	core/mm/pmap.c
	core/mm/slab2.c
	core/mm/tmpmap.c
	core/obj/device.c
	core/obj/fault.c
	core/obj/kso.c
	core/obj/objcopy.c
	core/obj/object.c
	core/obj/pageop.c
	core/obj/pager.c
	core/obj/pagevec.c
	core/obj/queue.c
	core/obj/range.c
	core/obj/rw.c
	core/obj/secctx.c
	core/obj/tie.c
	core/sys/kconf.c
	core/sys/object.c
	core/sys/thread.c
	core/sys/thread_sync.c
	lib/blake2.c
	lib/memory.c
	lib/qsort.c
	lib/rb.c
	lib/vsprintk.c
)

list(APPEND KERNEL_C_FLAGS
	"-nostdlibinc"
	"-ffreestanding"
	"-fno-omit-frame-pointer"
	"-std=gnu11"
	"-g"
	"-D__KERNEL__"
	"-Wall"
	"-Wextra"
	"-Wno-error=unused-variable"
	"-Wno-error=unused-function"
	"-Wno-error=unused-parameter"
	"-Wshadow"
	"-Wmissing-prototypes"
	"-DMP_NO_FILE"
	"-DLTC_NO_FILE"
	"-DLTM_DESC"
	"-DUSE_LTM"
	"-DARGTYPE=1"
	"-DCONFIG_SERIAL_DEBUG_STOPBITS=1"
	"-DCONFIG_SERIAL_DEBUG_WORDSZ=8"
	"-DCONFIG_SERIAL_DEBUG_ENABLE=y"
	"-DCONFIG_SERIAL_DEBUG_BAUD=38400"
	)

include(arch/${TWIZZLER_PROCESSOR}/include.cmake)
include(machine/${TWIZZLER_MACHINE}/include.cmake)

list(JOIN KERNEL_C_FLAGS " " KERNEL_C_FLAGS_SPACE)

ExternalProject_Add(
	"kerntommath"
	BUILD_IN_SOURCE ON
	CONFIGURE_COMMAND ""
	BUILD_COMMAND
		"make"
		"CFLAGS=-I${CMAKE_CURRENT_SOURCE_DIR}/include \
        -I${CMAKE_CURRENT_SOURCE_DIR}/arch/${TWIZZLER_PROCESSOR}/include \
		-I ${CMAKE_CURRENT_SOURCE_DIR}/machine/${TWIZZLER_MACHINE}/include \
		-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -fPIC -DUSE_LTM -DLTM_DESC \
		${KERNEL_C_FLAGS_SPACE} -DLTC_NO_FILE -DLTC_NO_ASM -DMP_NO_FILE -DARGTYPE=1 \
		-DXMALLOC=kalloc -DXFREE=kfree -DXCALLOC=kcalloc \
		-DMP_MALLOC=kalloc -DMP_FREE=kfree -DMP_REALLOC=krealloc -DMP_CALLOC=kcalloc \
		-DMP_NO_DEV_URANDOM -DMP_NO_FILE"
		"CC=${TOOLCHAIN_DIR}/bin/clang"
		"LD=${TOOLCHAIN_DIR}/bin/clang"
		"AR=ar"
		"RANLIB=ranlib"
	INSTALL_COMMAND ""
	SOURCE_DIR "${CMAKE_SOURCE_DIR}/tools/libtommath-kernel"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	BUILD_ALWAYS OFF
)

ExternalProject_Add(
	"kerntomcrypt"
	DEPENDS "kerntommath"
	BUILD_IN_SOURCE ON
	CONFIGURE_COMMAND ""
	BUILD_COMMAND
		"make"
		"V=1"
		"-j" "${BUILD_CORES}"
        "CFLAGS=-I${CMAKE_CURRENT_SOURCE_DIR}/include \
        -I${CMAKE_CURRENT_SOURCE_DIR}/arch/${TWIZZLER_PROCESSOR}/include \
		-I${CMAKE_CURRENT_SOURCE_DIR}/machine/${TWIZZLER_MACHINE}/include \
		-I${CMAKE_SOURCE_DIR}/tools/libtommath-kernel \
		-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -fPIC -DUSE_LTM -DLTM_DESC \
		${KERNEL_C_FLAGS_SPACE} -DLTC_NO_FILE -DLTC_NO_ASM -DMP_NO_FILE -DARGTYPE=1 \
		-DXMALLOC=kalloc -DXFREE=kfree -DXCALLOC=kcalloc"
		"CC=${TOOLCHAIN_DIR}/bin/clang"
		"LD=${TOOLCHAIN_DIR}/bin/clang"
		"AR=ar"
		"RANLIB=ranlib"
	INSTALL_COMMAND ""
	SOURCE_DIR "${CMAKE_SOURCE_DIR}/tools/libtomcrypt-kernel"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	BUILD_ALWAYS OFF
)

execute_process(COMMAND ${CMAKE_C_COMPILER} -target ${CMAKE_C_COMPILER_TARGET}
	-print-libgcc-file-name OUTPUT_VARIABLE COMPILER_RT OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${CMAKE_C_COMPILER} -target ${CMAKE_C_COMPILER_TARGET}
	-print-file-name=clang_rt.crtbegin-${TWIZZLER_PROCESSOR}.o OUTPUT_VARIABLE CRTBEGIN_OBJ OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${CMAKE_C_COMPILER} -target ${CMAKE_C_COMPILER_TARGET}
	-print-file-name=clang_rt.crtend-${TWIZZLER_PROCESSOR}.o OUTPUT_VARIABLE CRTEND_OBJ OUTPUT_STRIP_TRAILING_WHITESPACE)

add_executable(kernel ${KERNEL_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/link.ld)

add_dependencies(kernel kerntommath)
add_dependencies(kernel kerntomcrypt)

target_compile_options(kernel PRIVATE "SHELL:-include system.h")

# TODO: move flags to arch?
target_compile_options(kernel PRIVATE ${KERNEL_C_FLAGS})

set(CRTI_OBJ "${CMAKE_CURRENT_BINARY_DIR}/crti.o")
set(CRTN_OBJ "${CMAKE_CURRENT_BINARY_DIR}/crtn.o")

add_custom_command(
	OUTPUT ${CRTI_OBJ}
	MAIN_DEPENDENCY ${CRTI_SOURCE}
	COMMAND
	${CMAKE_C_COMPILER} ${CMAKE_C_FLAGS} ${KERNEL_C_FLAGS} -o ${CRTI_OBJ} -c ${CRTI_SOURCE}
)

add_custom_target(gen_crti DEPENDS ${CRTI_OBJ})

add_custom_command(
	OUTPUT ${CRTN_OBJ}
	MAIN_DEPENDENCY ${CRTN_SOURCE}
	COMMAND
	${CMAKE_C_COMPILER} ${CMAKE_C_FLAGS} ${KERNEL_C_FLAGS} -o ${CRTN_OBJ} -c ${CRTN_SOURCE}
)

add_custom_target(gen_crtn DEPENDS ${CRTN_OBJ})


add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/link.ld"
	MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/arch/${TWIZZLER_PROCESSOR}/link.ld.in"
	DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/machine/${TWIZZLER_MACHINE}/include/machine/memory.h"
	COMMAND
	"bash" "-c"
	"BASE=$(grep KERNEL_VIRTUAL_BASE \
	${CMAKE_CURRENT_SOURCE_DIR}/machine/${TWIZZLER_MACHINE}/include/machine/memory.h | sed -rn \
	's/#define KERNEL_VIRTUAL_BASE (0x[0-9a-fA-F])/\\1/p') && sed \
	s/%KERNEL_VIRTUAL_BASE%/$BASE/g \
	< ${CMAKE_CURRENT_SOURCE_DIR}/arch/${TWIZZLER_PROCESSOR}/link.ld.in > \
	${CMAKE_CURRENT_BINARY_DIR}/link.ld.tmp"
VERBATIM
COMMAND
	"bash" "-c"
	"BASE=$(grep KERNEL_PHYSICAL_BASE \
	${CMAKE_CURRENT_SOURCE_DIR}/machine/${TWIZZLER_MACHINE}/include/machine/memory.h | sed -rn \
	's/#define KERNEL_PHYSICAL_BASE (0x[0-9a-fA-F])/\\1/p') && sed \
	s/%KERNEL_PHYSICAL_BASE%/$BASE/g \
	< ${CMAKE_CURRENT_BINARY_DIR}/link.ld.tmp > \
	${CMAKE_CURRENT_BINARY_DIR}/link.ld.tmp2"
VERBATIM
COMMAND
	"bash" "-c"
	"BASE=$(grep KERNEL_LOAD_OFFSET \
	${CMAKE_CURRENT_SOURCE_DIR}/machine/${TWIZZLER_MACHINE}/include/machine/memory.h | sed -rn \
	's/#define KERNEL_LOAD_OFFSET (0x[0-9a-fA-F])/\\1/p') && sed \
	s/%KERNEL_LOAD_OFFSET%/$BASE/g \
	< ${CMAKE_CURRENT_BINARY_DIR}/link.ld.tmp2 > \
	${CMAKE_CURRENT_BINARY_DIR}/link.ld.tmp3"
VERBATIM
COMMAND "mv"
"${CMAKE_CURRENT_BINARY_DIR}/link.ld.tmp3"
"${CMAKE_CURRENT_BINARY_DIR}/link.ld"
COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/link.ld"
)

add_custom_target(kernel_ldscript DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/link.ld")

add_dependencies(kernel kernel_ldscript)
add_dependencies(kernel gen_crti)
add_dependencies(kernel gen_crtn)

SET(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> \
    <LINK_FLAGS> \
	${CRTI_OBJ} \
	${CRTBEGIN_OBJ} \
	<OBJECTS> -o <TARGET> <LINK_LIBRARIES> \
	${CMAKE_SOURCE_DIR}/tools/libtomcrypt-kernel/libtomcrypt.a \
	${CMAKE_SOURCE_DIR}/tools/libtommath-kernel/libtommath.a \
	${CRTEND_OBJ} \
	${CRTN_OBJ}")



target_link_options(kernel PRIVATE
	"-mcmodel=kernel"
	"-Wl,-z,max-page-size=0x1000"
	"-Wl,-z,common-page-size=0x1000"
	"-nostdlib"
	"-Wl,--no-dynamic-linker"
	"-T" "${CMAKE_CURRENT_BINARY_DIR}/link.ld"
)

set_target_properties(kernel PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/link.ld;${CRTI_OBJ};${CRTN_OBJ})

#target_link_libraries(kernel ${CMAKE_SOURCE_DIR}/tools/libtomcrypt-kernel/libtomcrypt.a)
#target_link_libraries(kernel ${CMAKE_SOURCE_DIR}/tools/libtommath-kernel/libtommath.a)
target_link_libraries(kernel "${COMPILER_RT}")

target_include_directories(kernel PRIVATE ${CMAKE_SOURCE_DIR}/tools/libtommath-kernel)
target_include_directories(kernel PRIVATE ${CMAKE_SOURCE_DIR}/tools/libtomcrypt-kernel/src/headers)
target_include_directories(kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/arch/${TWIZZLER_PROCESSOR}/include)
target_include_directories(kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/machine/${TWIZZLER_MACHINE}/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/twz/include)

set_target_properties(kernel PROPERTIES LINK_FLAGS "-static")

install(TARGETS kernel DESTINATION ../boot)

