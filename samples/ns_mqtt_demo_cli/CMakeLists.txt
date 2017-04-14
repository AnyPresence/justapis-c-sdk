cmake_minimum_required(VERSION 3.5)
project(ns-mqtt-demo-cmake-cli)

# Adding justapis-c-sdk. 
# Second parameter is just required since our library is out-of-tree of current CMAKE project.
# You can skip the second param if justapis-c-sdk is a sub-directory of your project.
add_subdirectory (../.. justapis-c-sdk_cmake_bin_dir) 
 
# add the executable
set(NS_MQTT_DEMO_CMAKE_CLI_SOURCE_FILES src/main.c)
add_executable (ns-mqtt-demo-cmake-cli ${NS_MQTT_DEMO_CMAKE_CLI_SOURCE_FILES})
target_link_libraries (ns-mqtt-demo-cmake-cli justapis-c-sdk -ldl -lm)#-ldl -lm is required e.g on Linux(Ubuntu 16.04 LTS)
target_include_directories (ns-mqtt-demo-cmake-cli PRIVATE ../../include ../../include/mosquitto/lib)