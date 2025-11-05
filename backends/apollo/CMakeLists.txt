# Copyright (C) 2025-2026 EHTech(Beijing)Co.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Makefile for a backend that generates code for Apollo target
# compiling for the simple_switch target.

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/tuna_nic/version.h.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/tuna_nic/version.h" @ONLY)

# Sources for tuna_nic backend executable
set (APOLLO_TUNA_NIC_SRCS
    tuna_nic/main.cpp
    tuna_nic/midend.cpp
    tuna_nic/midend.h
    tuna_nic/tunaNic.cpp
    tuna_nic/tunaNic.h
    tuna_nic/tunaProgramStructure.cpp
    tuna_nic/tunaProgramStructure.h
    tuna_nic/options.cpp
    tuna_nic/options.h
    )

set (APOLLO_BACKEND_COMMON_SRCS
    common/JsonObjects.cpp
    common/action.cpp
    common/controlFlowGraph.cpp
    common/deparser.cpp
    common/expression.cpp
    common/extern.cpp
    common/globals.cpp
    common/header.cpp
    common/helpers.cpp
    common/lower.cpp
    common/parser.cpp
    )

set (PORTABLE_COMMON
    portable_common/portable.cpp
    portable_common/portable.h
    portable_common/options.cpp
    portable_common/options.h
    portable_common/midend.cpp
    portable_common/midend.h
    )


set (APOLLO_BACKEND_COMMON_HDRS
  common/JsonObjects.h
  common/action.h
  common/annotations.h
  common/backend.h
  common/check_unsupported.h
  common/control.h
  common/controlFlowGraph.h
  common/deparser.h
  common/expression.h
  common/extern.h
  common/globals.h
  common/header.h
  common/helpers.h
  common/lower.h
  common/midend.h
  common/options.h
  common/parser.h
  common/sharedActionSelectorCheck.h
  )

set (IR_DEF_FILES ${IR_DEF_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/apollo.def PARENT_SCOPE)

# Create Apollo backend library with BMV2 dependencies
add_library(apollobackend STATIC ${APOLLO_BACKEND_COMMON_SRCS})
target_link_libraries(apollobackend ir-generated frontend backends-common bmv2backend)

# Create portable-common library
add_library(apollo-portable-common STATIC ${PORTABLE_COMMON})
target_link_libraries(apollo-portable-common apollobackend)

# Set libtunabackend.a
add_library(tunalib STATIC IMPORTED)
set_target_properties(tunalib PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/tuna_nic/tunalib/libtunabackend.a
)

# Create tuna switch executable
add_executable(p4c-apollo-tuna ${APOLLO_TUNA_NIC_SRCS})
include_directories(${CMAKE_SOURCE_DIR}/thrid-party)
target_link_libraries (p4c-apollo-tuna apollo-portable-common tunalib ${P4C_LIBRARIES} ${P4C_LIB_DEPS})

# Install both executables
install(TARGETS p4c-apollo-tuna RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tuna_nic/p4include
  DESTINATION ${P4C_ARTIFACTS_OUTPUT_DIRECTORY})

file(RELATIVE_PATH
    CURRENT_BINARY_DIR_PATH_REL
    ${P4C_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

file(RELATIVE_PATH
    P4C_BINARY_DIR_PATH_REL
    ${CMAKE_CURRENT_BINARY_DIR}
    ${P4C_BINARY_DIR}
)

# hack to get around the fact that the test scripts expect the backend
# binary to be in the top level directory. This should go away when we
# remove automake and fix the scripts.
add_custom_target(linkapollo
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CURRENT_BINARY_DIR_PATH_REL}/p4c-apollo-tuna ${P4C_BINARY_DIR}/p4c-apollo-tuna
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${P4C_BINARY_DIR_PATH_REL}/p4include ${CMAKE_CURRENT_BINARY_DIR}/p4include
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${P4C_BINARY_DIR_PATH_REL}/p4_14include ${CMAKE_CURRENT_BINARY_DIR}/p4_14include
  DEPENDS update_includes
  )
add_dependencies(p4c_driver linkapollo)
