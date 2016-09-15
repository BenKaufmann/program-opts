cmake_minimum_required(VERSION 3.1)
project(program_options VERSION 1.0 LANGUAGES CXX)

file(GLOB LIBOPTS_SRC src/*.cpp)
file(GLOB LIBOPTS_INC program_opts/*.h)
file(GLOB LIBOPTS_DTL program_opts/detail/*.h)

# create library
add_library(program_opts
	${LIBOPTS_INC}
	${LIBOPTS_DTL}
	${LIBOPTS_SRC}
)

# setup directories 
set(include_dest  "include/program_opts-${PROJECT_VERSION}")
set(main_lib_dest "lib/program_opts-${PROJECT_VERSION}")
set(lib_dest "${main_lib_dest}/${CMAKE_BUILD_TYPE}")

target_include_directories(program_opts PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:${include_dest}>
)

# installation & export
install(TARGETS program_opts EXPORT program_opts DESTINATION "${lib_dest}")
install(FILES ${LIBOPTS_INC} DESTINATION "${include_dest}/program_opts/")
install(FILES ${LIBOPTS_DTL} DESTINATION "${include_dest}/program_opts/detail")
install(FILES program_opts-config.cmake DESTINATION ${main_lib_dest})
install(EXPORT program_opts DESTINATION "${lib_dest}")

# enables cmake testing
enable_testing()

file(GLOB TEST_SRC tests/*.cpp)
add_executable(program_opts_test ${TEST_SRC})
target_link_libraries(program_opts_test program_opts)
add_test(test_program_opts program_opts_test)
target_include_directories(program_opts_test  PRIVATE 
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)


