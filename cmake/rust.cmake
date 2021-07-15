
function(add_rust_target name path)

add_custom_target(${name}
	ALL
	DEPENDS musl
	DEPENDS twz-bootstrap-2
	DEPENDS twix-bootstrap-2
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${path}
	COMMENT "cargo build ${name}"
	COMMAND
	${CMAKE_COMMAND} -E env
	"CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/${name}-cargo-build"
	"RUSTC=${TOOLCHAIN_DIR}/bin/rustc"
	"RUSTFLAGS=--target=${TWIZZLER_TRIPLE} -C linker=${TOOLCHAIN_DIR}/bin/clang -C link-arg=--sysroot=${SYSROOT_DIR}"
	cargo build ${CARGO_BUILD_TYPE} --color=always
	VERBATIM
)

	#\"CARGO_TERM_VERBOSE=true\" \
install(CODE "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${path} COMMAND ${CMAKE_COMMAND} -E env \
	\"CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/${name}-cargo-build\" \
	\"RUSTC=${TOOLCHAIN_DIR}/bin/rustc\" \
	\"RUSTFLAGS=--target=${TWIZZLER_TRIPLE} -C linker=${TOOLCHAIN_DIR}/bin/clang -C link-arg=--sysroot=${SYSROOT_DIR}\" \
	\"CARGO_INSTALL_ROOT=${SYSROOT_DIR}/usr\" \
	cargo install --path .)"
)

endfunction()

