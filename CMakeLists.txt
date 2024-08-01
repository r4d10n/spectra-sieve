cmake_minimum_required(VERSION 3.0)
project(spectra_sieve)

set(CMAKE_CXX_STANDARD 11)  # Adjust if necessary

set(SRC_DIR src)
set(BUILD_DIR build)

file(GLOB SOURCES "${SRC_DIR}/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE
  -I${CMAKE_CURRENT_SOURCE_DIR}/include
  -DUSE_WEBSOCKET
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
  pthread dl rt iio fftw3f m
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/libcivetweb.a
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
