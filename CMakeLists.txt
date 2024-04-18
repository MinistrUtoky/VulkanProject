# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project (vulkan_base)

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

add_subdirectory(third_party)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

add_subdirectory(src)

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

#==============================================================================
# COMPILE SHADERS
#

add_subdirectory(shaders)
add_subdirectory(assets)

function(add_shaders TARGET_NAME)
  set(SHADER_SOURCE_FILES ${ARGN}) # the rest of arguments to this function will be assigned as shader source files
  
  # Validate that source files have been passed
  list(LENGTH SHADER_SOURCE_FILES FILE_COUNT)
  if(FILE_COUNT EQUAL 0)
    message(FATAL_ERROR "Cannot create a shaders target without any source files")
  endif()

  set(SHADER_COMMANDS)
  set(SHADER_PRODUCTS)

  foreach(SHADER_SOURCE IN LISTS SHADER_SOURCE_FILES)
    #cmake_path(ABOSULTE_PATH SHADER_SOURCE NORMALIZE)
    cmake_path(GET SHADER_SOURCE FILENAME SHADER_NAME)
    
    # Build command
    list(APPEND SHADER_COMMAND "COMMAND ")
    list(APPEND SHADER_COMMAND "${glslc_executable} ")
    list(APPEND SHADER_COMMAND "${SHADER_SOURCE}")
    list(APPEND SHADER_COMMAND " -o ")
    list(APPEND SHADER_COMMAND "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")

    # Add product
    list(APPEND SHADER_PRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
  endforeach()
  message(${SHADER_COMMAND})
  add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv
  	DEPENDS ${TARGET_NAME}
	COMMAND ${glslc_executable} ${SHADER_SOURCE} -o ${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv
  )
  add_custom_target(${TARGET_NAME} ALL 
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv"
	COMMENT "gay"
  ) 	
endfunction()

add_shaders(theTarget ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sky.comp)