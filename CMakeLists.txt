cmake_minimum_required(VERSION 3.16)
project(cppvpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CPPVPP_BUILD_TESTS "Build unit tests" ON)
option(CPPVPP_BUILD_EXAMPLES "Build examples" ON)

# Generate C++ message definitions from VPP's *.api.json files.
set(CPPVPP_API_JSON_DIR "/usr/share/vpp/api" CACHE PATH
    "Directory containing VPP *.api.json files")
find_program(GO_EXECUTABLE go REQUIRED)
set(CPPVPP_GEN_BIN "${CMAKE_CURRENT_BINARY_DIR}/gen_api")
set(CPPVPP_GEN_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_api")
file(GLOB_RECURSE CPPVPP_API_JSON "${CPPVPP_API_JSON_DIR}/*.api.json")
if(NOT CPPVPP_API_JSON)
  message(FATAL_ERROR "No *.api.json files found in ${CPPVPP_API_JSON_DIR}")
endif()
set(CPPVPP_GEN_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(CPPVPP_GEN_DIR "${CPPVPP_GEN_ROOT}/vpp/api")
set(CPPVPP_API_HEADERS "")
foreach(j ${CPPVPP_API_JSON})
  get_filename_component(n "${j}" NAME)
  string(REGEX REPLACE "\\.json$" ".h" h "${n}")
  list(APPEND CPPVPP_API_HEADERS "${CPPVPP_GEN_DIR}/${h}")
endforeach()
add_custom_command(
  OUTPUT ${CPPVPP_API_HEADERS}
  COMMAND ${GO_EXECUTABLE} build -o ${CPPVPP_GEN_BIN} .
  COMMAND ${CPPVPP_GEN_BIN} ${CPPVPP_API_JSON_DIR} ${CPPVPP_GEN_DIR}
  WORKING_DIRECTORY ${CPPVPP_GEN_SRC_DIR}
  DEPENDS ${CPPVPP_API_JSON} ${CPPVPP_GEN_SRC_DIR}/main.go ${CPPVPP_GEN_SRC_DIR}/go.mod
  COMMENT "Generating C++ API headers from ${CPPVPP_API_JSON_DIR}")
add_custom_target(cppvpp_api_gen DEPENDS ${CPPVPP_API_HEADERS})

add_library(cppvpp
  src/codec.cc
  src/status.cc
  src/connection.cc
  src/vpe.cc)
add_library(cppvpp::cppvpp ALIAS cppvpp)
add_dependencies(cppvpp cppvpp_api_gen)
target_include_directories(cppvpp PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CPPVPP_GEN_ROOT}>
  $<INSTALL_INTERFACE:include>)
target_compile_options(cppvpp PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(CPPVPP_BUILD_EXAMPLES)
  add_executable(show_version examples/show_version.cc)
  target_link_libraries(show_version PRIVATE cppvpp)
  target_compile_options(show_version PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(CPPVPP_BUILD_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
  foreach(t codec status connection vpe api)
    add_executable(${t}_test tests/${t}_test.cc)
    target_link_libraries(${t}_test PRIVATE cppvpp GTest::gtest_main)
    target_compile_options(${t}_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME ${t}_test COMMAND ${t}_test)
  endforeach()
  add_test(NAME gen_api_test COMMAND ${GO_EXECUTABLE} test ./...
           WORKING_DIRECTORY ${CPPVPP_GEN_SRC_DIR})
endif()

install(TARGETS cppvpp ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY ${CPPVPP_GEN_ROOT}/vpp DESTINATION include)
