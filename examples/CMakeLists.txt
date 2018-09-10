add_executable(remi-test-client client.c)
target_link_libraries(remi-test-client remi)

add_executable(remi-test-client-dir client-dir.c)
target_link_libraries(remi-test-client-dir remi)

add_executable(remi-test-server server.c)
target_link_libraries(remi-test-server remi)
