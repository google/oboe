#
# Copyright 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

set(oboe_version "1.1.1")
set(oboe_url "https://github.com/google/oboe/archive/${oboe_version}.zip")
set(oboe_md5 "85b7cefe6046f17f8c7cf62762cbb5c7")

set(oboe_name ${CMAKE_STATIC_LIBRARY_PREFIX}oboe${CMAKE_STATIC_LIBRARY_SUFFIX})

ExternalProject_Add(oboe
  GIT_REPOSITORY https://github.com/NewProggie/oboe
  GIT_TAG fix_cmake_install_targets
  # URL ${oboe_url}
  # URL_MD5 ${oboe_md5}
  INSTALL_DIR ${HelloCMakeOboe_INSTALL_PREFIX}/oboe
  UPDATE_COMMAND ""
  CMAKE_ARGS -Wno-dev ${HelloCMakeOboe_DEFAULT_ARGS}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
  BUILD_BYPRODUCTS <INSTALL_DIR>/lib/${oboe_name})

ExternalProject_Add_Step(oboe CopyLibToJniFolder
  COMMAND ${CMAKE_COMMAND} -E make_directory
    ${HelloCMakeOboe_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/
  COMMAND ${CMAKE_COMMAND} -E copy
    ${HelloCMakeOboe_INSTALL_PREFIX}/oboe/lib/${oboe_name}
    ${HelloCMakeOboe_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/
  DEPENDEES install)

ExternalProject_Get_Property(oboe install_dir)
set(OBOE_ROOT ${install_dir} CACHE INTERNAL "")

file(MAKE_DIRECTORY ${OBOE_ROOT}/include)
set(OBOE_INCLUDE_DIR ${OBOE_ROOT}/include)
set(OBOE_LIBRARY ${OBOE_ROOT}/lib/${oboe_name})

add_library(Oboe::Oboe STATIC IMPORTED)
add_dependencies(Oboe::Oboe oboe)
set_target_properties(Oboe::Oboe PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES ${OBOE_INCLUDE_DIR}
  IMPORTED_LINK_INTERFACE_LIBRARIES OpenSLES
  IMPORTED_LOCATION ${OBOE_LIBRARY})

