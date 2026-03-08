add_executable(cdl_example basic_usage.cpp)
target_link_libraries(cdl_example PRIVATE cdl)

add_executable(cdl_tutorial tutorial.cpp)
target_link_libraries(cdl_tutorial PRIVATE cdl)

add_executable(cdl_file_manager file_manager.cpp)
target_link_libraries(cdl_file_manager PRIVATE cdl)
