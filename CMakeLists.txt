cmake_minimum_required( VERSION 3.11 FATAL_ERROR )
set(CMAKE_CXX_STANDARD 17)
project(xRooFit)

option(ENABLE_TESTS "Enable compilation of tests" OFF)

if(APPLE)
    add_definitions(-DAPPLE)
endif()

add_custom_target(xRooFit_version
        COMMENT "Generating xRooFitVersion.h ..."
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/versioning
        COMMAND ${CMAKE_COMMAND} -DOUTPUT_FILE=${CMAKE_BINARY_DIR}/versioning/temp_xroofit_version.h
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/versioning.cmake
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/versioning/temp_xroofit_version.h
        ${CMAKE_BINARY_DIR}/versioning/xRooFitVersion.h
        COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/versioning/temp_xroofit_version.h)

list(APPEND CMAKE_PREFIX_PATH $ENV{ROOTSYS})
# if 6.26 ROOT available, add RooFitHS3
find_package( ROOT 6.26 QUIET COMPONENTS MathCore RIO Core Tree Hist TreePlayer RooFit RooFitCore Matrix Gpad Graf RooStats HistPainter HistFactory Gui Ged RooFitHS3)
if (NOT ROOT_FOUND)
    find_package( ROOT 6.22 COMPONENTS MathCore RIO Core Tree Hist TreePlayer RooFit RooFitCore Matrix Gpad Graf RooStats HistPainter HistFactory Gui Ged )
endif()
message(STATUS "Using ROOT From: ${ROOT_INCLUDE_DIRS}")
include(${ROOT_USE_FILE})

include_directories(${ROOT_INCLUDE_DIRS})
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(${ROOT_CXX_FLAGS})



file(GLOB HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} xRooFit/*.h)
file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cxx)

ROOT_GENERATE_DICTIONARY(G__xRooFit ${HEADERS} LINKDEF src/LinkDef.h)


add_library(xRooFit SHARED ${SOURCES} G__xRooFit )
target_link_libraries(xRooFit ${ROOT_LIBRARIES})

target_sources(xRooFit PRIVATE ${CMAKE_BINARY_DIR}/versioning/xRooFitVersion.h)
set_source_files_properties(${CMAKE_BINARY_DIR}/versioning/xRooFitVersion.h PROPERTIES GENERATED TRUE)
target_include_directories(xRooFit BEFORE PRIVATE ${CMAKE_BINARY_DIR}/versioning)
add_dependencies(xRooFit xRooFit_version)



if (ENABLE_TESTS)
    add_subdirectory(test)
endif()


set(SETUP ${CMAKE_CURRENT_BINARY_DIR}/setup.sh)
file(WRITE ${SETUP} "#!/bin/bash\n")
file(APPEND ${SETUP} "# this is an auto-generated setup script\n" )
file(APPEND ${SETUP} "export PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}:\${PYTHONPATH}\n")
file(APPEND ${SETUP} "export ROOT_INCLUDE_PATH=${CMAKE_CURRENT_SOURCE_DIR}:\${ROOT_INCLUDE_PATH}\n")
file(APPEND ${SETUP} "export LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}:${ROOT_LIBRARY_DIR}:\${LD_LIBRARY_PATH}\n")
file(APPEND ${SETUP} "export DYLD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}:\${DYLD_LIBRARY_PATH}\n")
## file(APPEND ${SETUP} "export PATH=\${PATH}:${CMAKE_CURRENT_SOURCE_DIR}/scripts\n")
string(ASCII 27 Esc)
message(STATUS "${Esc}[1;35mIMPORTANT: In future sessions you just have to remember to run:
    source ${SETUP}${Esc}[m" )

install( DIRECTORY xRooFit DESTINATION include FILES_MATCHING
        COMPONENT headers
        PATTERN "*.h"
        )

install( FILES ${CMAKE_CURRENT_BINARY_DIR}/libxRooFit.rootmap
        ${CMAKE_CURRENT_BINARY_DIR}/libxRooFit_rdict.pcm
        DESTINATION lib
        COMPONENT libraries)

install(TARGETS xRooFit
        LIBRARY DESTINATION lib
        COMPONENT libraries)
