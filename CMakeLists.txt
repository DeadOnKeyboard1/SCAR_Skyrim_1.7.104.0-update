cmake_minimum_required(VERSION 3.25)
cmake_policy(SET CMP0141 NEW)
option(SCAR_PHASE1_BUILD "Build through the shared SKSE standard" OFF)
if(SCAR_PHASE1_BUILD)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/vcpkg-installed/x64-windows-static-md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/vcpkg/installed/x64-windows-static-md")
    file(TO_CMAKE_PATH "$ENV{USERPROFILE}" privacy_prefix)
    add_compile_options("/experimental:deterministic" "/pathmap:${privacy_prefix}=$<ANGLE-R>DoK<")
endif()


set(CommonLibPath "${CMAKE_SOURCE_DIR}/extern/CommonLibSSE-NG" CACHE PATH "Path to CommonLibSSE-NG")
set(COMMONLIB_PREBUILT OFF CACHE BOOL "Build the selected exact source revision" FORCE)
set(ENABLE_SKYRIM_SE ON CACHE BOOL "Enable Skyrim SE support" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "Enable Skyrim AE support" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "Original DLL rejects VR" FORCE)
# standards & flags
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT SCAR_PHASE1_BUILD)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "")
endif()

# info
project(
	SCAR
	VERSION 1.0.9
	LANGUAGES CXX
)
include(GNUInstallDirs)
add_compile_options(/utf-8)

# out-of-source builds only
if(${PROJECT_SOURCE_DIR} STREQUAL ${PROJECT_BINARY_DIR})
	message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# in-place configuration
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in
	${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
	@ONLY
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	@ONLY
)

# source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.h")
source_group(
	TREE 
		${CMAKE_CURRENT_SOURCE_DIR}
	FILES 
		${SOURCES}
)

source_group(
	TREE ${CMAKE_CURRENT_BINARY_DIR}
	FILES ${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
)

# dependencies
if(SCAR_PHASE1_BUILD)
    if(NOT EXISTS "$ENV{SKSE_COMMONLIB_SOURCE_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR "Use Build-SCAR.bat through SKSEBuildStandard")
    endif()
    set(CommonLibPath "$ENV{SKSE_COMMONLIB_SOURCE_ROOT}")
    set(COMMONLIB_ENABLE_IPO OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "CommonLib internal tests are not the SCAR tests" FORCE)
    set(COMMONLIB_PREBUILT_DIR "" CACHE PATH "" FORCE)
    set(FETCHCONTENT_SOURCE_DIR_HDE64 "${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/minhook-v1.3.4" CACHE PATH "Cached HDE64" FORCE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
    add_subdirectory(${CommonLibPath} CommonLibSSE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/DKUtil" DKUtil)
else()
    add_subdirectory(${CommonLibPath} CommonLibSSE EXCLUDE_FROM_ALL)
    add_subdirectory($ENV{DKUtilPath} DKUtil)
endif()
find_package(spdlog CONFIG REQUIRED)

# cmake target
# runtime
add_library(
	${PROJECT_NAME}
	SHARED
		${SOURCES}
		${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
		${CMAKE_CURRENT_BINARY_DIR}/version.rc
		.clang-format
		vcpkg.json
)

# include dir
target_include_directories(
	${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/effolkronium-random-1.5.0/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/minhook-v1.3.4/src/hde
        ${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/vcpkg-installed/x64-windows-static-md/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/vcpkg/installed/x64-windows-static-md/include
)

target_compile_definitions(
	${PROJECT_NAME}
	PRIVATE
		DKU_MAX_REFLECTION_ENUM=16
		GLM_ENABLE_EXPERIMENTAL
)

# linkage
target_link_libraries(
	${PROJECT_NAME} 
	PRIVATE
		CommonLibSSE::CommonLibSSE
		DKUtil::DKUtil
		spdlog::spdlog
)

# compiler def
if (MSVC)
	add_compile_definitions(_UNICODE)

	target_compile_options(
		${PROJECT_NAME}
		PRIVATE
			/W3
			/permissive-
			/Zc:alignedNew
			/Zc:auto
			/Zc:__cplusplus
			/Zc:externC
			/Zc:externConstexpr
			/Zc:forScope
			/Zc:hiddenFriend
			/Zc:implicitNoexcept
			/Zc:lambda
			/Zc:noexceptTypes
			/Zc:preprocessor
			/Zc:referenceBinding
			/Zc:rvalueCast
			/Zc:sizedDealloc
			/Zc:strictStrings
			/Zc:ternary
			/Zc:threadSafeInit
			/Zc:trigraphs
			/Zc:wchar_t
			/wd4200 # nonstandard extension used : zero-sized array in struct/union
	)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SKSEAPI=__cdecl)
    target_link_options(${PROJECT_NAME} PRIVATE /DEBUG:FULL /INCREMENTAL:NO /PDBALTPATH:%_PDB% /OPT:REF /OPT:ICF)
endif()

# PCH
target_precompile_headers(
	${PROJECT_NAME}
	PRIVATE
		src/PCH.h
)

# post build event
if(NOT MOD_AUTOMATED_BUILD AND NOT SCAR_PHASE1_BUILD)
	add_custom_command(
		TARGET
			${PROJECT_NAME}
		POST_BUILD
			COMMAND
				powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/!Update.ps1" "COPY" "${PROJECT_VERSION}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>" "${PROJECT_NAME}" "${ANNIVERSARY_EDITION}"
			COMMENT
				"Updating project :))"
	)
endif()

# called via !Rebuild.ps1
if(SCAR_PHASE1_BUILD)
    add_executable(scar_tests tests/LogicTests.cpp src/SCARActionData.cpp src/DataHandler.cpp src/Function.cpp
        src/DebugAPI/DebugAPI.cpp src/DebugAPI/Util.cpp src/DebugAPI/FocusObject.cpp)
    target_include_directories(scar_tests PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/effolkronium-random-1.5.0/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/minhook-v1.3.4/src/hde"
        "${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/vcpkg-installed/x64-windows-static-md/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/vcpkg/installed/x64-windows-static-md/include")
    target_compile_definitions(scar_tests PRIVATE SKSEAPI=__cdecl GLM_ENABLE_EXPERIMENTAL)
    target_precompile_headers(scar_tests PRIVATE src/PCH.h)
    target_link_libraries(scar_tests PRIVATE CommonLibSSE::CommonLibSSE DKUtil::DKUtil spdlog::spdlog)
    set_property(TARGET scar_tests PROPERTY INTERPROCEDURAL_OPTIMIZATION OFF)
    add_executable(scar_trampoline_tests EXCLUDE_FROM_ALL build-hooks/trampoline-probe.cpp)
    target_include_directories(scar_trampoline_tests PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/effolkronium-random-1.5.0/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/vcpkg-installed/x64-windows-static-md/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain/vcpkg/installed/x64-windows-static-md/include")
    target_compile_definitions(scar_trampoline_tests PRIVATE SKSEAPI=__cdecl GLM_ENABLE_EXPERIMENTAL)
    target_link_libraries(scar_trampoline_tests PRIVATE CommonLibSSE::CommonLibSSE DKUtil::DKUtil spdlog::spdlog)
    set_property(TARGET scar_trampoline_tests PROPERTY INTERPROCEDURAL_OPTIMIZATION OFF)
endif()

if(DEFINED GROUP)
set_target_properties(
	${PROJECT_NAME}
	PROPERTIES
	FOLDER
	${GROUP}
)
endif()
