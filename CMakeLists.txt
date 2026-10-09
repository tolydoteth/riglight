cmake_minimum_required(VERSION 3.20)
project(riglight VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra)

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp)
add_library(riglight_core STATIC ${CORE_SOURCES})
target_include_directories(riglight_core PUBLIC src third_party/iCUESDK/include)
# _LIB strips __declspec(dllimport) from iCUESDK.h: we only use its types and load the DLL at runtime.
target_compile_definitions(riglight_core PUBLIC _LIB)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
add_executable(core_tests ${TEST_SOURCES})
target_include_directories(core_tests PRIVATE tests)
target_link_libraries(core_tests PRIVATE riglight_core)
target_link_options(core_tests PRIVATE -static)

enable_testing()
add_test(NAME core_tests COMMAND core_tests)

if(WIN32)
  file(GLOB WIN_SOURCES CONFIGURE_DEPENDS src/win/*.cpp)
  if(WIN_SOURCES)
    add_executable(RigLight WIN32 ${WIN_SOURCES})
    target_compile_definitions(RigLight PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
                               _WIN32_WINNT=0x0A00 WINVER=0x0A00 _WIN32_IE=0x0A00)
    target_link_libraries(RigLight PRIVATE riglight_core setupapi hid user32 gdi32 shell32 advapi32
                          ole32 comdlg32 d2d1 dwrite uuid)
    target_link_options(RigLight PRIVATE -static -municode "$<$<CONFIG:Release>:-s>")
    add_custom_command(TARGET RigLight POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${CMAKE_SOURCE_DIR}/third_party/iCUESDK/redist/x64/iCUESDK.x64_2019.dll
              $<TARGET_FILE_DIR:RigLight>)
  endif()
endif()
