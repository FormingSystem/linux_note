cmake_minimum_required(VERSION 3.22)

project(loop_native_service VERSION 0.2.0 LANGUAGES C CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOOP_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

set(ENABLE_PROGRAMS OFF CACHE BOOL "Do not build Mbed TLS programs" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "Do not build Mbed TLS tests" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "Do not promote dependency warnings" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "Build static Mbed TLS libraries" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "Build static Mbed TLS libraries" FORCE)
FetchContent_Declare(
  mbedtls
  URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.4/mbedtls-3.6.4.tar.bz2
  URL_HASH SHA256=ec35b18a6c593cf98c3e30db8b98ff93e8940a8c4e690e66b41dfc011d678110
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(mbedtls)
if(TARGET mbedtls)
  set_target_properties(mbedtls PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
if(TARGET mbedx509)
  set_target_properties(mbedx509 PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

set(LIBUV_BUILD_SHARED OFF CACHE BOOL "Build only the static libuv library" FORCE)
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "Do not build libuv tests" FORCE)
set(LIBUV_BUILD_BENCH OFF CACHE BOOL "Do not build libuv benchmarks" FORCE)
FetchContent_Declare(
  libuv
  URL https://dist.libuv.org/dist/v1.51.0/libuv-v1.51.0.tar.gz
  URL_HASH SHA256=5f0557b90b1106de71951a3c3931de5e0430d78da1d9a10287ebc7a3f78ef8eb
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(libuv)

function(loop_enable_strict_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /EHsc)
  else()
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Werror
    )
  endif()
endfunction()

function(loop_enable_sanitizers target)
  if(LOOP_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  endif()
endfunction()

add_library(loop_native_protocol STATIC
  src/protocol/framing.cpp
  src/protocol/messages.cpp
  src/service/workspace_service.cpp
  src/support/portable_crypto.cpp
)
target_include_directories(loop_native_protocol PUBLIC src)
target_link_libraries(loop_native_protocol PUBLIC nlohmann_json::nlohmann_json MbedTLS::mbedcrypto uv_a)
loop_enable_strict_warnings(loop_native_protocol)
loop_enable_sanitizers(loop_native_protocol)

add_executable(loop_native_service src/service/main.cpp)
target_link_libraries(loop_native_service PRIVATE loop_native_protocol)
loop_enable_strict_warnings(loop_native_service)
loop_enable_sanitizers(loop_native_service)

enable_testing()
add_executable(loop_native_protocol_tests tests/protocol_tests.cpp)
target_link_libraries(loop_native_protocol_tests PRIVATE loop_native_protocol)
loop_enable_strict_warnings(loop_native_protocol_tests)
loop_enable_sanitizers(loop_native_protocol_tests)
add_test(NAME loop_native_protocol_tests COMMAND loop_native_protocol_tests)

add_executable(loop_native_workspace_tests tests/workspace_tests.cpp)
target_link_libraries(loop_native_workspace_tests PRIVATE loop_native_protocol)
loop_enable_strict_warnings(loop_native_workspace_tests)
loop_enable_sanitizers(loop_native_workspace_tests)
add_test(NAME loop_native_workspace_tests COMMAND loop_native_workspace_tests)
