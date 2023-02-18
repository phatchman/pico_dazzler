cmake_minimum_required(VERSION 3.13)

function(add_git_submodule_recurse dir)
# add a Git submodule directory to CMake, assuming the
# Git submodule directory is a CMake project.
#
# Usage: in CMakeLists.txt
# 
# include(AddGitSubmodule.cmake)
# add_git_submodule_recurse(mysubmod_dir)

find_package(Git REQUIRED)
if (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 3.18)
  if(NOT EXISTS ${dir}/CMakeLists.txt)
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive -- ${dir}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMAND_ERROR_IS_FATAL ANY)
  endif()
else()
  if(NOT EXISTS ${dir}/CMakeLists.txt)
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive -- ${dir}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  endif()
endif()

endfunction(add_git_submodule_recurse)

function(add_git_submodule dir)
# add a Git submodule directory to CMake, assuming the
# Git submodule directory is a CMake project.
#
# Usage: in CMakeLists.txt
# 
# include(AddGitSubmodule.cmake)
# add_git_submodule(mysubmod_dir)

find_package(Git REQUIRED)

if (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 3.18)
  if(NOT EXISTS ${dir}/CMakeLists.txt)
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init -- ${dir}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMAND_ERROR_IS_FATAL ANY)
  endif()
else ()
  if (NOT EXISTS ${dir}/CMakeLists.txt)
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init -- ${dir}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  endif()
endif()

endfunction(add_git_submodule)
