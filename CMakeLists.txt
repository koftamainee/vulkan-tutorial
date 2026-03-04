cmake_minimum_required(VERSION 3.20)
project(vulkan LANGUAGES CXX)

function (add_slang_shader_target TARGET)
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
            -entry VertMain -entry FragMain
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

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_slang_shader_target(shaders SOURCES ${CMAKE_CURRENT_LIST_DIR}/shaders/shader.slang)

add_executable(vulkan src/main.cpp
        src/FVulkanTriangle.cpp
        src/FVulkanTriangle.h
        src/EResult.h
)

add_dependencies(vulkan shaders)

target_compile_definitions(vulkan PRIVATE
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)

target_include_directories(vulkan PRIVATE
        "src"
)

add_custom_target(run_asan
        COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/asan.supp
        $<TARGET_FILE:vulkan>
        DEPENDS vulkan
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

target_link_libraries(vulkan PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
)

