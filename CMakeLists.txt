cmake_minimum_required(VERSION 3.21)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
if(POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
endif()

if(POLICY CMP0063)
    cmake_policy(SET CMP0063 NEW)
endif()

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

if(NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
    SET(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

include(VcpkgIntegration)
project(metamod-vconsole)
include(CompilerRuntime)

list(APPEND SOURCES_LIST
	"src/h_export.cpp"
	"src/meta_api.cpp"
	"src/dllapi.cpp"
	"src/engine_api.cpp"
	"src/vconsole_server.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SOURCES_LIST})

find_path(HLSDK_DIRECTORY "cl_dll/GameStudioModelRenderer.h" PATH_SUFFIXES "hlsdk")
find_path(METAMOD_DIRECTORY "common/BaseSystemModule.h" PATH_SUFFIXES "metamod")

target_include_directories(${PROJECT_NAME} PRIVATE
	"${HLSDK_DIRECTORY}/common"
	"${HLSDK_DIRECTORY}/dlls"
	"${HLSDK_DIRECTORY}/engine"
	"${HLSDK_DIRECTORY}/game_shared"
	"${HLSDK_DIRECTORY}/pm_shared"
	"${HLSDK_DIRECTORY}/public"
	"${METAMOD_DIRECTORY}"
	"src"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	OUTPUT_NAME "metamod-vconsole"
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

# compiler options
if(WIN32)
	target_link_options(${PROJECT_NAME} PRIVATE /SUBSYSTEM:CONSOLE)
	target_compile_definitions(${PROJECT_NAME} PRIVATE
		_CRT_SECURE_NO_WARNINGS=1
	)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -Wfatal-errors)
	target_compile_options(${PROJECT_NAME} PRIVATE -fpermissive)

	# Architecture-specific flags
	if(NOT VCPKG_TARGET_TRIPLET MATCHES "^x64")
		# x86 build
		target_compile_options(${PROJECT_NAME} PRIVATE -m32)
		target_link_options(${PROJECT_NAME} PRIVATE -m32)
	endif()

	# Use static C++ runtime to avoid conflicts with old libs in HLDS directories
	target_link_options(${PROJECT_NAME} PRIVATE -static-libstdc++ -static-libgcc)
endif()

# link platform-specific libraries
if(NOT WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE dl pthread)
endif()

# set build output directory
set(DIR_COMMON_OUTPUT
	${CMAKE_BINARY_DIR}/$<CONFIG>/bin
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	ARCHIVE_OUTPUT_DIRECTORY ${DIR_COMMON_OUTPUT}
	LIBRARY_OUTPUT_DIRECTORY ${DIR_COMMON_OUTPUT}
	RUNTIME_OUTPUT_DIRECTORY ${DIR_COMMON_OUTPUT}
)

install(TARGETS ${PROJECT_NAME}
	DESTINATION "${CMAKE_INSTALL_PREFIX}"
	PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
	    GROUP_READ GROUP_EXECUTE
	    WORLD_READ WORLD_EXECUTE
)
