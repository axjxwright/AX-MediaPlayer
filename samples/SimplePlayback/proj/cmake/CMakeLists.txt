cmake_minimum_required( VERSION 3.10 FATAL_ERROR )
set( CMAKE_VERBOSE_MAKEFILE ON )

project( SimplePlaybackApp )

get_filename_component( APP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../" ABSOLUTE )
get_filename_component( CINDER_PATH "${APP_PATH}/../../../../" ABSOLUTE )
get_filename_component( BLOCK_PATH "${APP_PATH}/../.." ABSOLUTE )

include("${CINDER_PATH}/proj/cmake/modules/cinderMakeApp.cmake")

set ( THE_BLOCKS "AX-MediaPlayer")

ci_make_app(
	SOURCES "${APP_PATH}/src/SimplePlaybackApp.cxx"
	CINDER_PATH ${CINDER_PATH}
	BLOCKS ${BLOCK_PATH}
)

add_definitions( -DCINDER_PATH="${CINDER_PATH}" )