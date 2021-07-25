# Get the latest abbreviated commit hash of the working branch
execute_process(
        COMMAND git describe --always --dirty --tags
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
configure_file(${SOURCE_DIR}/version.h.in ${OUTPUT_FILE})
