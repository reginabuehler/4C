# This file is part of 4C multiphysics licensed under the
# GNU Lesser General Public License v3.0 or later.
#
# See the LICENSE.md file in the top-level for license information.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

### Function to execute a process and check for errors
function(_execute_process)

  set(options "")
  set(oneValueArgs "")
  set(multiValueArgs PROCESS_COMMAND)
  cmake_parse_arguments(
    _parsed
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
    )

  if(DEFINED _parsed_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "There are unparsed arguments: ${_parsed_UNPARSED_ARGUMENTS}!")
  endif()

  if(NOT DEFINED _parsed_PROCESS_COMMAND)
    message(FATAL_ERROR "Process command is a necessary input argument!")
  endif()

  execute_process(
    COMMAND ${_parsed_PROCESS_COMMAND}
    RESULT_VARIABLE _command_result
    OUTPUT_VARIABLE _command_output
    ERROR_VARIABLE _command_error
    )

  if(NOT _command_result EQUAL "0")
    message(
      STATUS
        "Failed to execute command '${_parsed_PROCESS_COMMAND}' during setup of virtual Python environment for building and testing!"
      )
    message(STATUS "Output: ${_command_output}")
    message(STATUS "Result: ${_command_result}")
    message(FATAL_ERROR "Error: ${_command_error}")
  endif()

endfunction()

### Set up a virtual Python environment for building and testing 4C

message(STATUS "Setting up virtual Python environment for building and testing 4C")
find_package(Python REQUIRED COMPONENTS Interpreter Development)
if(Python_FOUND)
  message(STATUS "Using python executable: ${Python_EXECUTABLE} (V${Python_VERSION})")
endif()

if(Python_VERSION VERSION_LESS "3.10")
  message(
    FATAL_ERROR
      "Python version must be at least 3.10, but found ${Python_VERSION}. "
      "Please install a newer version of Python and set FOUR_C_PYTHON_ROOT to the correct path."
    )
endif()

set(FOUR_C_PYTHON_VENV_BUILD "${PROJECT_BINARY_DIR}/python_venv_build_test")
set(FOUR_C_PYTHON_VENV_BUILD
    "${PROJECT_BINARY_DIR}/python_venv_build_test"
    PARENT_SCOPE
    )

_execute_process(
  PROCESS_COMMAND
  ${Python_EXECUTABLE}
  -m
  venv
  ${FOUR_C_PYTHON_VENV_BUILD}
  )
_execute_process(
  PROCESS_COMMAND
  ${FOUR_C_PYTHON_VENV_BUILD}/bin/pip
  install
  pip==24.3.1
  wheel==0.45.0
  )
_execute_process(
  PROCESS_COMMAND
  ${FOUR_C_PYTHON_VENV_BUILD}/bin/pip
  install
  -r
  ${PROJECT_SOURCE_DIR}/cmake/python/requirements.txt
  )

message(
  STATUS
    "Successfully created Python virtual environment in ${FOUR_C_PYTHON_VENV_BUILD} and installed necessary requirements"
  )
