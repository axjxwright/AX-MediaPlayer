if( NOT TARGET AX-MediaPlayer )

	if ( NOT WIN32 )
		error("This platform is not supported (yet)")
	endif()

	get_filename_component( AXMP_SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../src" ABSOLUTE )
	get_filename_component( CINDER_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE )

	file(GLOB_RECURSE AXMP_SOURCE_FILES
      "${AXMP_SOURCE_PATH}/*.h"
      "${AXMP_SOURCE_PATH}/*.cxx"
	 )

	add_library( AX-MediaPlayer ${AXMP_SOURCE_FILES} )

	target_include_directories( AX-MediaPlayer PUBLIC "${AXMP_SOURCE_PATH}" )
	target_include_directories( AX-MediaPlayer SYSTEM BEFORE PUBLIC "${CINDER_PATH}/include" )

	if( NOT TARGET cinder )
		    include( "${CINDER_PATH}/proj/cmake/configure.cmake" )
		    find_package( cinder REQUIRED PATHS
		        "${CINDER_PATH}/${CINDER_LIB_DIRECTORY}"
		        "$ENV{CINDER_PATH}/${CINDER_LIB_DIRECTORY}" )
	endif()
	target_link_libraries( AX-MediaPlayer PRIVATE cinder )
	
endif()



