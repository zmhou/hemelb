
# This file is part of HemeLB and is Copyright (C)
# the HemeLB team and/or their institutions, as detailed in the
# file AUTHORS. This software is provided under the terms of the
# license in the file LICENSE.
cmake_minimum_required (VERSION 2.8)
Project(HemeLBMaster)
include(ExternalProject)

#------- Conditional compile options ----------

OPTION(TIXML_USE_STL "Use STL with TIXML" ON)
if(TIXML_USE_STL)
	add_definitions(-DTIXML_USE_STL)
endif(TIXML_USE_STL)

option(HEMELB_USE_DEBUGGER "Use built in hemelb debugger" OFF)
option(HEMELB_VALIDATE_GEOMETRY "Validate geometry" OFF)
set(HEMELB_READING_GROUP_SIZE 5
  CACHE INTEGER "Number of cores to use to read geometry file.")
set(HEMELB_LOG_LEVEL Info
	CACHE STRING "Log level, choose 'Critical', 'Error', 'Warning', 'Info', 'Debug' or 'Trace'" )
set(HEMELB_STEERING_LIB basic
	CACHE STRING "Steering library, choose 'basic' or 'none'" )
option(HEMELB_USE_MULTIMACHINE "Use multi-level parallelism support" OFF)
option(HEMELB_BUILD_UNITTESTS "Build the unit-tests" ON)
option(HEMELB_USE_STREAKLINES "Calculate streakline images" OFF)
option(HEMELB_USE_ALL_WARNINGS_GNU "Show all compiler warnings on development builds (gnu-style-compilers)" ON)
option(HEMELB_STATIC_ASSERT "Use simple compile-time assertions" ON)
set(HEMELB_OPTIMISATION "-O4" CACHE STRING "Optimisation level (can be blank or -O1 to -O4)")
option(HEMELB_BUILD_MULTISCALE "Build HemeLB Multiscale functionality" OFF)
set(HEMELB_LATTICE "D3Q15"
  CACHE STRING "Select the lattice type to use (D3Q15,D3Q19,D3Q27)")
set(HEMELB_KERNEL "LBGK"
  CACHE STRING "Select the kernel to use (LBGK,EntropicAnsumali,EntropicChik,MRT,NNCY,NNC,NNTPL)")
set(HEMELB_WALL_BOUNDARY "SIMPLEBOUNCEBACK"
  CACHE STRING "Select the boundary conditions to be used at the walls (FINTERPOLATION,GZS,SIMPLEBOUNCEBACK,JUNKYANG)")
set(HEMELB_STEERING_HOST "CCS" CACHE STRING "Use a default host suffix for steering? (CCS, NGS2Leeds, NGS2Manchester, LONI, NCSA or blank)")
option(HEMELB_DEPENDENCIES_SET_RPATH "Set runtime RPATH" ON)
set(HEMELB_SUBPROJECT_MAKE_JOBS 1 CACHE INTEGER "Number of jobs to use for subproject build steps")
option(HEMELB_WAIT_ON_CONNECT "Wait for steering client" OFF)
option(HEMELB_IMAGES_TO_NULL "Write images to null" OFF) 
option(HEMELB_USE_SSE3 "Use SSE3 intrinsics" OFF)
set(HEMELB_COMPUTE_ARCHITECTURE "AMDBULLDOZER"
  CACHE STRING "Select the architecture of the machine being used (INTELSANDYBRIDGE,AMDBULLDOZER,NEUTRAL,ISBFILEVELOCITYINLET)")
option(HEMELB_USE_VELOCITY_WEIGHTS_FILE "Use Velocity weights file" OFF)

#------- Dependencies -----------

add_subdirectory(dependencies)
	
# ------ HemeLB ------------------
message("${PROJECT_SOURCE_DIR}/Code")
ExternalProject_Add(
   hemelb
	URL ${PROJECT_SOURCE_DIR}/Code	
	CONFIGURE_COMMAND ${CMAKE_COMMAND} <SOURCE_DIR>
        -DBOOST_ROOT=${BOOST_ROOT}
	-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} 
	-DHEMELB_DEPENDENCIES_PATH=${CMAKE_CURRENT_SOURCE_DIR}/dependencies
	-DHEMELB_DEPENDENCIES_INSTALL_PATH=${HEMELB_DEPENDENCIES_INSTALL_PATH}
	-DTIXML_USE_STL=${TIXML_USE_STL}
	-DHEMELB_USE_DEBUGGER=${HEMELB_USE_DEBUGGER}
        -DHEMELB_VALIDATE_GEOMETRY=${HEMELB_VALIDATE_GEOMETRY}
	-DHEMELB_LOG_LEVEL=${HEMELB_LOG_LEVEL}
	-DHEMELB_STEERING_LIB=${HEMELB_STEERING_LIB}
	-DHEMELB_USE_MULTIMACHINE=${HEMELB_USE_MULTIMACHINE}
	-DHEMELB_BUILD_UNITTESTS=${HEMELB_BUILD_UNITTESTS}
	-DHEMELB_USE_STREAKLINES=${HEMELB_USE_STREAKLINES}
	-DHEMELB_OPTIMISATION=${HEMELB_OPTIMISATION}
	-DHEMELB_READING_GROUP_SIZE=${HEMELB_READING_GROUP_SIZE}
	-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
	-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
	-DMPI_C_COMPILER=${MPI_C_COMPILER}
	-DMPI_CXX_COMPILER=${MPI_CXX_COMPILER}
	-DMPI_C_NO_INTERROGATE=${MPI_C_NO_INTERROGATE}
	-DMPI_CXX_NO_INTERROGATE=${MPI_CXX_NO_INTERROGATE}
	-DHEMELB_USE_ALL_WARNINGS_GNU=${HEMELB_USE_ALL_WARNINGS_GNU}
	-DHEMELB_STEERING_HOST=${HEMELB_STEERING_HOST}
	-DCTEMPLATE_USE_STATIC=${CTEMPLATE_USE_STATIC}
	-DCPPUNIT_USE_STATIC=${CPPUNIT_USE_STATIC}
	-DHEMELB_DEPENDENCIES_SET_RPATH=${HEMELB_DEPENDENCIES_SET_RPATH}
	-DHEMELB_STATIC_ASSERT=${HEMELB_STATIC_ASSERT}
        -DHEMELB_LATTICE=${HEMELB_LATTICE}
        -DHEMELB_KERNEL=${HEMELB_KERNEL}
        -DHEMELB_WALL_BOUNDARY=${HEMELB_WALL_BOUNDARY}
	-DHEMELB_WAIT_ON_CONNECT=${HEMELB_WAIT_ON_CONNECT}
	-DHEMELB_BUILD_MULTISCALE=${HEMELB_BUILD_MULTISCALE}
	-DHEMELB_IMAGES_TO_NULL=${HEMELB_IMAGES_TO_NULL}
        -DHEMELB_USE_SSE3=${HEMELB_USE_SSE3}
    -DHEMELB_COMPUTE_ARCHITECTURE=${HEMELB_COMPUTE_ARCHITECTURE}
    -DHEMELB_USE_VELOCITY_WEIGHTS_FILE=${HEMELB_USE_VELOCITY_WEIGHTS_FILE}
	BUILD_COMMAND make -j${HEMELB_SUBPROJECT_MAKE_JOBS}
)

# These are handled in dependencies/CMakeLists.txt and needed here
# to ensure they are build in correct order.
add_dependencies(hemelb cppunit parmetis tinyxml boost ctemplate zlib mpwide)
