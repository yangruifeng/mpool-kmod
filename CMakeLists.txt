#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2015-2020 Micron Technology, Inc.  All rights reserved.
#

cmake_minimum_required(VERSION 3.2)
if(${CMAKE_MAJOR_VERSION} GREATER 2)
cmake_policy(SET CMP0046 OLD)
endif()

set (CMAKE_C_STANDARD 99)

message(STATUS "Configuring mpool-kmod project")

# used to generate RPM package names
project( mpool-kmod )

set( CMAKE_INSTALL_PREFIX /usr )

##################
# The next three variables determine where the build outputs land:
#
#   MPOOL_EXT_INSTALL_PREFIX
#     - used by external code in ./sub directory.
#
#   MPOOL_STAGE_INSTALL_PREFIX
#     - used by mpool-kmod code.
#
#   MPOOL_TREE_SCRIPTS_DIR
#     - Scripts related to the mpool-kmod source tree (building it, working
#       with git, etc.)
#
# Build outputs for mpool-kmod and external code are kept separate primarily so
# developers can rebuild just the mpool-kmod code which is much quicker than
# building everything.
##################

set( MPOOL_EXT_INSTALL_PREFIX          ${PROJECT_BINARY_DIR}/ext_install)
set( MPOOL_STAGE_INSTALL_PREFIX        ${PROJECT_BINARY_DIR}/stage)
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY  ${MPOOL_STAGE_INSTALL_PREFIX}/bin)
set( MPOOL_TREE_SCRIPTS_DIR            ${PROJECT_SOURCE_DIR}/scripts)

# Install() destination for CPack
set( MPOOL_TOOLS_BIN                   /opt/micron/bin)

# Docs and Example Code
set( MPOOL_DOCS_DIR                    share/doc/mpool/)
set( MPOOL_UDOC_DIR                    /opt/micron/mpool/doc)

# A kmod that should always be present in MPOOL_KERNEL_DIR from which we
# can obtain the value of uname -r.
set( MPOOL_MODINFO_FILE                virtio_blk.ko)

string(LENGTH ${PROJECT_SOURCE_DIR} MPOOL_PROJECT_SOURCE_DIR_PATH_LEN)

set( MPOOL_INCLUDE_DIRS     ${PROJECT_SOURCE_DIR}/include)

include_directories(${MPOOL_EXT_INSTALL_PREFIX}/include)
include_directories(${MPOOL_STAGE_INSTALL_PREFIX}/include)

# increasing warning levels
set( CFLAGS_WARN_1 "${CFLAGS_WARN_0} -Wall -Werror -std=gnu99")

if ( "${MPOOL_DISTRO}" STREQUAL  "el6" )
   set( CFLAGS_WARN_2 "${CFLAGS_WARN_1} -Wlogical-op -Wno-missing-field-initializers  -Wuninitialized -Wno-format-zero-length")
else()
   set( CFLAGS_WARN_2 "${CFLAGS_WARN_1} -Wlogical-op -Wno-missing-field-initializers  -Wuninitialized -Wmaybe-uninitialized ")
endif()

set( CFLAGS_WARN_3 "${CFLAGS_WARN_2} -Wextra -Wno-conversion -Wno-sign-conversion -Wno-sign-compare -Wno-unused-parameter")
set( CFLAGS_WARN_4 "${CFLAGS_WARN_2} -Wextra -Wconversion    -Wsign-conversion    -Wsign-compare    -Wunused-parameter")
set( CFLAGS_WARN_MAX "${CFLAGS_WARN_4}")

set( CMAKE_C_FLAGS "${CFLAGS_WARN_3} -g3")

if ( ${UBSAN} )
    message(STATUS "Enabling ubsan support")
    add_compile_options( -fno-omit-frame-pointer )
    add_compile_options( -fsanitize=undefined -fno-sanitize=alignment )
    link_libraries( -fsanitize=undefined -fno-sanitize=alignment )
endif()

if ( ${ASAN} )
    message(STATUS "Enabling asan/lsan support")
    add_compile_options( -fno-omit-frame-pointer )
    add_compile_options( -fsanitize=address )
    link_libraries( -fsanitize=address )
endif()


# Generate some macros that can be used to recognize a distro kernel.
# These need to be passed to kernel modules too, hence building them as
# CMAKE_KC_FLAGS and then merging them into CMAKE_C_FLAGS.
#
# Always make our distro variables visible as macros via cflags
#
if ( "${MPOOL_DISTRO}" STREQUAL  "el7" )
   set ( CMAKE_KC_FLAGS "${CMAKE_KC_FLAGS} -DMPOOL_DISTRO_EL7  ")
   if ( ${MPOOL_KDIR_UNAME} MATCHES "3.10.0-1062" )
      set ( CMAKE_KC_FLAGS "${CMAKE_KC_FLAGS} -DMPOOL_DISTRO_EL77 ")
   elseif ( ${MPOOL_KDIR_UNAME} MATCHES "3.10.0-862" )
      set ( CMAKE_KC_FLAGS "${CMAKE_KC_FLAGS} -DMPOOL_DISTRO_EL75 ")
   elseif ( ${MPOOL_KDIR_UNAME} MATCHES "3.10.0-693" )
      set ( CMAKE_KC_FLAGS "${CMAKE_KC_FLAGS} -DMPOOL_DISTRO_EL74 ")
   else()
       message(FATAL_ERROR "Unsupported el7 kernel: ${MPOOL_KDIR_UNAME}")
   endif()
elseif ( "${MPOOL_DISTRO}" STREQUAL  "el8" )
   set ( CMAKE_KC_FLAGS "${CMAKE_KC_FLAGS} -DMPOOL_DISTRO_EL8  ")
   if ( ${MPOOL_KDIR_UNAME} MATCHES "4.18.0-147" )
      set ( CMAKE_KC_FLAGS "${CMAKE_KC_FLAGS} -DMPOOL_DISTRO_EL81 ")
   elseif ( ${MPOOL_KDIR_UNAME} MATCHES "4.18.0-193" )
      set ( CMAKE_KC_FLAGS "${CMAKE_KC_FLAGS} -DMPOOL_DISTRO_EL82 ")
   else()
       message(FATAL_ERROR "Unsupported el8 kernel: ${MPOOL_KDIR_UNAME}")
   endif()
elseif ( "${MPOOL_DISTRO}" STREQUAL  "el6")
   set ( CMAKE_KC_FLAGS "${CMAKE_KC_FLAGS} -DMPOOL_DISTRO_EL6 ")
elseif ( "${MPOOL_DISTRO}" STREQUAL  "fc25")
   set ( CMAKE_KC_FLAGS "${CMAKE_KC_FLAGS} -DMPOOL_DISTRO_FC25 ")
elseif ( "${MPOOL_DISTRO}" STREQUAL  "fc26")
   set ( CMAKE_KC_FLAGS "${CMAKE_KC_FLAGS} -DMPOOL_DISTRO_FC26 ")
elseif ( "${MPOOL_DISTRO}" STREQUAL  "fc27")
   set ( CMAKE_KC_FLAGS "${CMAKE_KC_FLAGS} -DMPOOL_DISTRO_FC27 ")
endif()

set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_KC_FLAGS}")


# Fedora system packages use Python 3; RHEL7 packages use Python 2
if( EXISTS /etc/fedora-release )
    set( MPOOL_PYTHON /usr/bin/python3 )
else()
    set( MPOOL_PYTHON /usr/bin/python )
endif()

execute_process( COMMAND ${MPOOL_PYTHON} -c "from distutils.sysconfig import get_python_lib; print(get_python_lib())" OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE MPOOL_PYTHON_PKG_DIR )

get_filename_component( MPOOL_PYTHON_BASE_DIR ${MPOOL_PYTHON_PKG_DIR}/.. ABSOLUTE )
set ( MPOOL_PYTHON_SOS_DIR ${MPOOL_PYTHON_PKG_DIR}/sos )
set ( MPOOL_PYTHON_SOS_PLUGINS_DIR ${MPOOL_PYTHON_PKG_DIR}/sos/plugins )


include(CMakeLists.macros)


# Set-up what kind of build this is
#
if( "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )
  add_definitions( -DMPOOL_BUILD_DEBUG )
  add_definitions( -DDEBUG_RCU )
  set( MPOOL_BUILD_TYPE_FLAG "d" )

elseif( "${CMAKE_BUILD_TYPE}" STREQUAL "OptDebug" )
  add_definitions( -DMPOOL_BUILD_DEBUG )
  set( MPOOL_BUILD_TYPE_FLAG "od" )

elseif( "${CMAKE_BUILD_TYPE}" STREQUAL "Release" )
  add_definitions( -DMPOOL_BUILD_RELEASE )
  add_definitions( -DNDEBUG )
  set( MPOOL_BUILD_TYPE_FLAG "r" )

elseif( "${CMAKE_BUILD_TYPE}" STREQUAL "RelAssert" )
  add_definitions( -DMPOOL_BUILD_RELASSERT )
  set( MPOOL_BUILD_TYPE_FLAG "ra" )

elseif( "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo" )
  add_definitions( -DMPOOL_BUILD_RELEASE )
  add_definitions( -DNDEBUG )
  set( MPOOL_BUILD_TYPE_FLAG "rd" )

else()
  message( FATAL_ERROR "Invalid build type [${CMAKE_BUILD_TYPE}]" )
endif()


################################################################
#
# Create the version header file
#
################################################################

# We use git
#
find_package(Git)

# Edit these lines when we cut a release branch.
set( MPOOL_VERSION_MAJOR 0 )
set( MPOOL_VERSION_MINOR 0 )
set( MPOOL_VERSION_PATCH 0 )

# Create our template for the header file
#
file(WRITE ${CMAKE_BINARY_DIR}/mpool_version.h.in
"#define MPOOL_VERSION_LEN 61\n"
"static const char mpool_version[MPOOL_VERSION_LEN] = \"@MPOOL_VERSION@\";\n"
"static const char mpool_branch[] = \"@MPOOL_BRANCH@\";\n"
"static const char mpool_tag[] = \"@MPOOL_TAG@\";\n"
"static const char mpool_buildserver[] = \"@MPOOL_BUILDSERVER@\";\n"
)

# Create cmake code to transform input template into target header file.
#
file(WRITE ${CMAKE_BINARY_DIR}/mpool_version.cmake
"set(MPOOL_SHA ${MPOOL_SHA})
 if( NOT DEFINED MPOOL_SHA )
   execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      OUTPUT_VARIABLE MPOOL_SHA
      OUTPUT_STRIP_TRAILING_WHITESPACE
   )
 endif()
 execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --dirty --always --tags
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE MPOOL_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
 )
 execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE MPOOL_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
 )
 execute_process(
    COMMAND date +%Y%m%d
    OUTPUT_VARIABLE MPOOL_DATESTAMP
    OUTPUT_STRIP_TRAILING_WHITESPACE
 )
 execute_process(
    COMMAND hostname -f
    OUTPUT_VARIABLE MPOOL_BUILDSERVER
    OUTPUT_STRIP_TRAILING_WHITESPACE
 )
 execute_process(
    COMMAND dirname ${MPOOL_KERNEL_DIR}
    OUTPUT_VARIABLE MPOOL_KERNEL_PARENT_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
 )
 find_file(
   MPOOL_MODINFO_FILE_PATH
   NAMES ${MPOOL_MODINFO_FILE} ${MPOOL_MODINFO_FILE}.xz
   PATHS
     \${MPOOL_KERNEL_PARENT_DIR}/kernel/drivers/block
     \${MPOOL_KERNEL_DIR}/drivers/block
   NO_DEFAULT_PATH
 )
 execute_process(
  COMMAND bash -c \"/sbin/modinfo \${MPOOL_MODINFO_FILE_PATH} | grep vermagic | awk '{print $2}'\"
  OUTPUT_VARIABLE MPOOL_KMOD_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
 )

 set( MPOOL_KERNEL_VER \${MPOOL_KMOD_VERSION})

 #
 # cmake DIST doesn't work in 3.6.2.
 #
 execute_process(
   COMMAND rpm --eval %{dist}
  OUTPUT_VARIABLE PACKAGE_DIST
  OUTPUT_STRIP_TRAILING_WHITESPACE
 )
 set( MPOOL_KMOD_DIST \${PACKAGE_DIST})

 #
 # In 3.6.x, for some reason, CPACK_RPM_PACKAGE_ARCHITECTURE is an
 # empty string when this CMakeLists.txt gets processed.
 #
 execute_process(
  COMMAND uname -m
  OUTPUT_VARIABLE PACKAGE_ARCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
 )

 if( \${MPOOL_TAG} MATCHES \"dirty$\" )
  set( MPOOL_DIRTYTAG \".dirty\" )
 endif( )

# Keep this line--we need it in release branches
# set( MPOOL_MAJORMINORPATCH ${MPOOL_VERSION_MAJOR}.${MPOOL_VERSION_MINOR}.${MPOOL_VERSION_PATCH} )
 set( MPOOL_MAJORMINORPATCH \"master\" )

 set( MPOOL_RELEASE ${MPOOL_BUILD_TYPE_FLAG}${BUILD_NUMBER}.\${MPOOL_DATESTAMP}.\${MPOOL_SHA}\${MPOOL_DIRTYTAG} )

 if (REL_CANDIDATE)
  set( MPOOL_RELEASE ${MPOOL_BUILD_TYPE_FLAG}${BUILD_NUMBER}.\${MPOOL_DATESTAMP} )
 endif()

 set( MPOOL_KMOD_RELEASE ${MPOOL_BUILD_TYPE_FLAG}${BUILD_NUMBER}.\${MPOOL_DATESTAMP}.\${MPOOL_SHA}\${MPOOL_DIRTYTAG}-\${MPOOL_KMOD_VERSION} )
 #
 # MPOOL_SHA varibale may not be populated when user is downloading
 # source code from tarball on the github website instead of cloning the repo
 #
 if( NOT MPOOL_SHA )
   set( MPOOL_KMOD_RELEASE ${MPOOL_BUILD_TYPE_FLAG}${BUILD_NUMBER}.\${MPOOL_DATESTAMP}\${MPOOL_DIRTYTAG}-\${MPOOL_KMOD_VERSION} )
   set( MPOOL_RELEASE ${MPOOL_BUILD_TYPE_FLAG}${BUILD_NUMBER}.\${MPOOL_DATESTAMP}\${MPOOL_DIRTYTAG} )
 endif()
 if (REL_CANDIDATE)
  set( MPOOL_KMOD_RELEASE ${MPOOL_BUILD_TYPE_FLAG}${BUILD_NUMBER}.\${MPOOL_DATESTAMP}-\${MPOOL_KMOD_VERSION} )
 endif()

 #
 # If the kernel version string includes the machine architecture,
 # strip it out of MPOOL_KMOD_VERSION, which we only use to generate the
 # package file name.  If we do not do this, we end up with stuff like
 # 'x86_64.x86_64.rpm'.
 #
 if( \${MPOOL_KMOD_RELEASE} MATCHES \${PACKAGE_ARCH})
  string(LENGTH \${MPOOL_KMOD_RELEASE} MPOOL_KMOD_RELEASE_LENGTH)
  string(LENGTH \${PACKAGE_ARCH} PACKAGE_ARCH_LENGTH)
  math(EXPR SUBSTRING_END_POS \"\${MPOOL_KMOD_RELEASE_LENGTH} - \${PACKAGE_ARCH_LENGTH} - 1\")
  string(SUBSTRING \${MPOOL_KMOD_RELEASE} 0 \${SUBSTRING_END_POS} MPOOL_KMOD_RELEASE)
 endif()

 #
 # Similar deal with dist.
 #
 if( \${MPOOL_KMOD_VERSION} MATCHES \${PACKAGE_DIST})
  unset( MPOOL_KMOD_DIST )
 endif()

 #
 # There's a little bit of horribleness in that Fedora/RedHat do not
 # permit hyphens in release strings.  We want the kernel version in
 # the release string in any RPM that contains a kernel module.  Our
 # compromise will be to substitute dots for hyphens in the kernel
 # module for the Release: field of the RPM, but for ease of scripting,
 # we'll leave the hyphen in the RPM file name.
 #
 execute_process(
  COMMAND bash -c \"echo \${MPOOL_KMOD_RELEASE} | sed 's/\-/./g'\"
  OUTPUT_VARIABLE MPOOL_KMOD_RPM_RELEASE
  OUTPUT_STRIP_TRAILING_WHITESPACE
 )

 set( MPOOL_VERSION \${MPOOL_MAJORMINORPATCH}-\${MPOOL_RELEASE} )
 set( MPOOL_KMOD_VERSION \${MPOOL_MAJORMINORPATCH}-\${MPOOL_KMOD_RELEASE} )

 if( NOT MPOOL_VERSION_CMAKE_INCLUDEONLY )
   if (EXISTS \${DST})
   	configure_file(\${SRC} \${CMAKE_BINARY_DIR}/mpool_version.h.tmp @ONLY)
   	file(SHA256 \${CMAKE_BINARY_DIR}/mpool_version.h.tmp NEWSHA)
   	file(SHA256 \${DST} OLDSHA)
   	# Replace mpool_version.h file with a newer only if the content changed.
   	if (NOT \${OLDSHA} EQUAL \${NEWSHA})
     		configure_file(\${SRC} \${DST} @ONLY)
   	endif()
   else()
     	configure_file(\${SRC} \${DST} @ONLY)
   endif()

 endif( )
"
)

# Set up custom target
#
add_custom_target(
    mpool_version
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MPOOL_STAGE_INSTALL_PREFIX}/install
    COMMAND ${CMAKE_COMMAND} -D SRC=${CMAKE_BINARY_DIR}/mpool_version.h.in
                     -D DST=${MPOOL_STAGE_INSTALL_PREFIX}/include/mpool_version.h
                     -P ${CMAKE_BINARY_DIR}/mpool_version.cmake
)


################################################################
#
# Establish prerequisites for all compiled targets
#
################################################################

set( MPOOL_SYNTH_PREREQS mpool_version )

################################################################
#
# Subdirectories
#
################################################################

enable_testing()

add_subdirectory( src )
add_subdirectory( scripts/modules-load.d )
add_subdirectory( scripts/sysconfig/modules )
add_subdirectory( scripts/udev/rules.d )
add_subdirectory( scripts/sysctl.d )


# NOTE: The package name and version values are configured at "make config"
# time.  If changes are made in the tree (thereby changing the last commit SHA),
# the configured package name won't change until the next make config.
#
# This *should* be OK for a while, since we expect our build server to clone a
# a fresh tree before every build.

set( MPOOL_VERSION_CMAKE_INCLUDEONLY "ON" )
include( ${CMAKE_BINARY_DIR}/mpool_version.cmake )
set( MPOOL_VERSION_CMAKE_INCLUDEONLY "OFF" )

# Package options
#
set( CPACK_GENERATOR "RPM" )
set( CPACK_PACKAGE_NAME "mpool-kmod" )
set( CPACK_PACKAGE_VERSION_MAJOR "${MPOOL_VERSION_MAJOR}" )
set( CPACK_PACKAGE_VERSION_MINOR "${MPOOL_VERSION_MINOR}" )
set( CPACK_PACKAGE_VERSION_PATCH "${MPOOL_VERSION_PATCH}" )
set( CPACK_RPM_PACKAGE_URL "https://github.com/hse-project/mpool-kmod" )
set( CPACK_RPM_PACKAGE_LICENSE "GPLv2 and MIT" )
set( CPACK_RPM_PACKAGE_GROUP "Unspecified" )
set( CPACK_RPM_PACKAGE_VENDOR "Unspecified" )


# Explicitly set CPACK_PACKAGE_VERSION first, otherwise CMake may generate
# nonsense version strings within generated spec files

set( CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}" )


set( CPACK_RPM_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}" )
set( CPACK_RPM_PACKAGE_RELEASE "${MPOOL_RELEASE}${PACKAGE_DIST}")
set( CPACK_RPM_COMPONENT_INSTALL "ON" )
set( CPACK_RPM_PACKAGE_RELOCATABLE "ON" )
set( CPACK_RPM_RELOCATION_PATHS /usr /etc )

# We don't want to claim ownership of these directories, lest there be
# conflicts.
#
set( CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
  /etc
  /etc/modules-load.d
  /etc/sysconfig
  /etc/sysconfig/modules
  /lib
  /usr/lib
  /usr/lib/sysctl.d
  /usr/lib/udev
  /usr/lib/udev/rules.d
  /usr/lib/modules
  /usr/lib/modules/${MPOOL_KERNEL_VER}
  ${MPOOL_PYTHON_BASE_DIR}
  ${MPOOL_PYTHON_PKG_DIR}
  ${MPOOL_PYTHON_SOS_DIR}
  ${MPOOL_PYTHON_SOS_PLUGINS_DIR}
)

# Setting this keeps CMake 3.7+ from generating nonsense filenames
set( CPACK_RPM_FILE_NAME "RPM-DEFAULT" )

#
# How to generate the RPM spec file template for your version of CMake, so it
# can be passed to CPACK_RPM_RUNTIME_USER_BINARY_SPECFILE:
#
#   cd builds/release.fc25  # (or whatever build/dist you're working on)
#   cpack -D CPACK_RPM_GENERATE_USER_BINARY_SPECFILE_TEMPLATE=1 -G RPM
#
# https://cmake.org/cmake/help/latest/cpack_gen/rpm.html#variable:CPACK_RPM_GENERATE_USER_BINARY_SPECFILE_TEMPLATE
#

SET( CPACK_COMPONENTS_ALL runtime )

# Runtime
#
set( CPACK_RPM_RUNTIME_PACKAGE_NAME "${CPACK_PACKAGE_NAME}" )
set( CPACK_RPM_RUNTIME_PACKAGE_SUMMARY
   "Object Storage Media Pool (mpool) kernel module")
set(
  CPACK_RPM_RUNTIME_PACKAGE_DESCRIPTION
  "Object Storage Media Pool (mpool) kernel module for kernel ${MPOOL_KMOD_VERSION}")
set( CPACK_RPM_RUNTIME_FILE_NAME "${CPACK_RPM_RUNTIME_PACKAGE_NAME}-${CPACK_RPM_PACKAGE_VERSION}-${MPOOL_KMOD_RELEASE}${MPOOL_KMOD_DIST}.${PACKAGE_ARCH}.rpm" )
set( CPACK_RPM_RUNTIME_PACKAGE_RELEASE "${CPACK_RPM_PACKAGE_RELEASE}")
set( CPACK_RPM_RUNTIME_USER_BINARY_SPECFILE "${MPOOL_TREE_SCRIPTS_DIR}/package/mpool.spec.in")
set( CPACK_RPM_RUNTIME_PRE_INSTALL_SCRIPT_FILE "${MPOOL_TREE_SCRIPTS_DIR}/package/rpm-pre-install.sh" )
set( CPACK_RPM_RUNTIME_POST_INSTALL_SCRIPT_FILE "${MPOOL_TREE_SCRIPTS_DIR}/package/rpm-post-install.sh" )
set( CPACK_RPM_RUNTIME_PRE_UNINSTALL_SCRIPT_FILE "${MPOOL_TREE_SCRIPTS_DIR}/package/rpm-pre-uninstall.sh" )
set( CPACK_RPM_RUNTIME_POST_UNINSTALL_SCRIPT_FILE "${MPOOL_TREE_SCRIPTS_DIR}/package/rpm-post-uninstall.sh" )
set( CPACK_RPM_RUNTIME_USER_FILELIST
  "%dir /usr/lib/modules/${MPOOL_KERNEL_VER}/mpool"
)
set( CPACK_RPM_RUNTIME_PACKAGE_REQUIRES "sos >= 3.2" )

include( CPack )

get_directory_property( DirIncs INCLUDE_DIRECTORIES )
get_directory_property( DirDefs COMPILE_DEFINITIONS )
get_directory_property( DirOpts COMPILE_OPTIONS )

message(STATUS "Compiler flags:")
message(STATUS "  INCLUDE_DIRECTORIES    = ${DirIncs}")
message(STATUS "  COMPILE_DEFINITIONS    = ${DirDefs}")
message(STATUS "  COMPILE_OPTIONS        = ${DirOpts}")
message(STATUS "  CMAKE_C_FLAGS          = ${CMAKE_C_FLAGS}")
message(STATUS "Add the following compiler flags based on build type:")
message(STATUS "  CMAKE_C_FLAGS_DEBUG          = ${CMAKE_C_FLAGS_DEBUG}")
message(STATUS "  CMAKE_C_FLAGS_RELEASE        = ${CMAKE_C_FLAGS_RELEASE}")
message(STATUS "  CMAKE_C_FLAGS_RELASSERT      = ${CMAKE_C_FLAGS_RELASSERT}")
message(STATUS "  CMAKE_C_FLAGS_RELWITHDEBINFO = ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
message(STATUS "  CMAKE_C_FLAGS_OPTDEBUG       = ${CMAKE_C_FLAGS_OPTDEBUG}")
message(STATUS "  CMAKE_KC_FLAGS               = ${CMAKE_KC_FLAGS}")
message(STATUS "Linker flags:")
message(STATUS "  CMAKE_EXE_LINKER_FLAGS = ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "Notes:")
message(STATUS "  * Build with: 'make VERBOSE=1' to verify compiler flags")
