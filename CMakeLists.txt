# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("fcgi-example")

# ------------------------ CHANGE PATH BELOW ON WINDOWS ------------------------
set(VCPKG_INC "/git/vcpkg/installed/x64-windows/include")
set(VCPKG_LIB "/git/vcpkg/installed/x64-windows/lib")
# ------------------------ CHANGE PATH ABOVE ON WINDOWS ------------------------

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(OS_SPECIFIC_LIBS wsock32 ws2_32 Userenv)
endif()

link_directories(${VCPKG_LIB})

add_executable(fcgi-example fcgi-example.cpp third-party/daemonize.cpp)
target_link_libraries(fcgi-example PRIVATE ${OS_SPECIFIC_LIBS} fcgi)
target_include_directories(fcgi-example PRIVATE "." "third-party" ${VCPKG_INC})
