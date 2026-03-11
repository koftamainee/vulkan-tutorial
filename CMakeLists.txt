cmake_minimum_required(VERSION 3.20)
project(vulkan LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)

function(add_slang_shader_target TARGET)
    cmake_parse_arguments(SHADER "" "" "SOURCES" ${ARGN})
    set(SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
    set(OUTPUT_SPV ${SHADERS_DIR}/slang.spv)
    if(NOT DEFINED SLANGC_EXECUTABLE)
        find_program(SLANGC_EXECUTABLE NAMES slangc)
        if(NOT SLANGC_EXECUTABLE)
            message(FATAL_ERROR "Slang compiler not found!")
        endif()
    endif()
    add_custom_command(
            OUTPUT ${OUTPUT_SPV}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
            COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES}
            -target spirv
            -profile spirv_1_4
            -emit-spirv-directly
            -fvk-use-entrypoint-name
            -entry vert_main -entry frag_main
            -o ${OUTPUT_SPV}
            WORKING_DIRECTORY ${SHADERS_DIR}
            DEPENDS ${SHADER_SOURCES}
            COMMENT "Compiling Slang shaders"
            VERBATIM
    )
    add_custom_target(${TARGET} DEPENDS ${OUTPUT_SPV})
endfunction()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(
        tinyobjloader
        GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
        GIT_TAG v2.0.0rc13
)


FetchContent_MakeAvailable(tinyobjloader)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_slang_shader_target(shaders
        SOURCES ${CMAKE_CURRENT_LIST_DIR}/shaders/shader.slang)

add_executable(vulkan
        src/main.cpp
        src/FVulkanApplication.cpp
        src/FVulkanApplication.h
        src/EResult.h
)

set_target_properties(vulkan PROPERTIES
        UNITY_BUILD ON
)

add_dependencies(vulkan shaders)

target_compile_options(vulkan PRIVATE
        -fno-exceptions
        -fno-rtti
        -Wall

        $<$<CONFIG:Debug>:
        -O0
        -g3
        >

        $<$<CONFIG:Release>:
        -O3
        -march=native
        -flto=auto
        -fomit-frame-pointer
        -DNDEBUG
        >

        $<$<CONFIG:RelWithDebInfo>:
        -O2
        -g
        -fno-omit-frame-pointer
        -flto=auto
        -DNDEBUG
        >
)

target_link_options(vulkan PRIVATE
        $<$<CONFIG:Release>:
        -flto=auto
        -Wl,-O1
        -Wl,--as-needed
        -Wl,-s
        >
        $<$<CONFIG:RelWithDebInfo>:
        -flto=auto
        -Wl,-O1
        -Wl,--as-needed
        >
)

target_include_directories(vulkan PRIVATE
        src
        ${stb_SOURCE_DIR}
)

target_link_libraries(vulkan PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
        tinyobjloader
)