cmake_minimum_required(VERSION 3.5)
project(justapis-c-sdk)

## Build Options

set(JA_ENABLE_RESPONSE_CACHE 1)
set(JA_ENABLE_CJSON 1)
set(JA_ENABLE_PUBLIC_KEY_PINNING 1)
set(JA_ENABLE_MQTT 1)
set(JA_ENABLE_MQTT_WITH_THREADING 1)

## -------------------------

## You can add '-Werror' after '-Wall' to turn all warning into errors.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall") 

## Make sure we have necessary 'CURL' dependency installed
FIND_PACKAGE(CURL)
IF(CURL_FOUND)
    INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIR})
    SET(requiredlibs ${requiredlibs} ${CURL_LIBRARIES} )
ELSE(CURL_FOUND)
    MESSAGE(FATAL_ERROR "Could not find the CURL library and development files.")
ENDIF(CURL_FOUND)


## Make sure we have necessary 'Threads' dependencies installed(if needed)
IF(JA_ENABLE_MQTT AND JA_ENABLE_MQTT_WITH_THREADING)
	SET(JA_NEEDS_THREADS TRUE)
ELSE()
	SET(JA_NEEDS_THREADS FALSE)
ENDIF()
MESSAGE(STATUS "JA_NEEDS_THREADS: " ${JA_NEEDS_THREADS})

IF(JA_NEEDS_THREADS)
	## Try Finding pthread
	set(CMAKE_THREAD_PREFER_PTHREAD ON)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package (Threads REQUIRED)

	MESSAGE(STATUS "CMAKE_USE_PTHREADS_INIT: " ${CMAKE_USE_PTHREADS_INIT})

	IF(NOT CMAKE_USE_PTHREADS_INIT)
		MESSAGE(FATAL_ERROR "Could not find the pthread library and header files.")
	ENDIF(NOT CMAKE_USE_PTHREADS_INIT)
ENDIF(JA_NEEDS_THREADS)

#Config File
set(JA_CONFIG_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/include/ja_config.h")
configure_file(include/ja_config.h.in ${JA_CONFIG_OUTPUT_PATH})
MESSAGE(STATUS "ja_config.h generated at:" ${JA_CONFIG_OUTPUT_PATH})

##
## Create the library target
##
set(LIB_SOURCE_FILES 
	include/ja_config.h
	include/mosquitto_config.h
	include/justapis.h src/justapis.c 
	include/cJSON.h src/cJSON.c
	include/mosquitto/lib/mosquitto.h src/mosquitto/lib/mosquitto.c
	include/mosquitto/lib/dummypthread.h
	include/mosquitto/lib/mqtt3_protocol.h
	include/mosquitto/lib/mosquitto_internal.h
	include/mosquitto/lib/logging_mosq.h src/mosquitto/lib/logging_mosq.c
	include/mosquitto/lib/memory_mosq.h src/mosquitto/lib/memory_mosq.c
	include/mosquitto/lib/messages_mosq.h src/mosquitto/lib/messages_mosq.c
	include/mosquitto/lib/net_mosq.h src/mosquitto/lib/net_mosq.c
	src/mosquitto/lib/read_handle_client.c
	src/mosquitto/lib/read_handle_shared.c
	include/mosquitto/lib/read_handle.h src/mosquitto/lib/read_handle.c
	src/mosquitto/lib/send_client_mosq.c
	include/mosquitto/lib/send_mosq.h src/mosquitto/lib/send_mosq.c
	include/mosquitto/lib/socks_mosq.h src/mosquitto/lib/socks_mosq.c
	src/mosquitto/lib/srv_mosq.c
	src/mosquitto/lib/thread_mosq.c
	include/mosquitto/lib/time_mosq.h src/mosquitto/lib/time_mosq.c
	include/mosquitto/lib/tls_mosq.h src/mosquitto/lib/tls_mosq.c
	include/mosquitto/lib/util_mosq.h src/mosquitto/lib/util_mosq.c
	include/mosquitto/lib/will_mosq.h src/mosquitto/lib/will_mosq.c)

#Source
add_library(justapis-c-sdk ${LIB_SOURCE_FILES})

#Linking
IF(JA_NEEDS_THREADS)
	target_link_libraries(justapis-c-sdk ${CURL_LIBRARIES} Threads::Threads)
ELSE(JA_NEEDS_THREADS)
	target_link_libraries(justapis-c-sdk ${CURL_LIBRARIES})
ENDIF(JA_NEEDS_THREADS)

#Headers
target_include_directories(justapis-c-sdk PUBLIC include include/mosquitto/lib)

##
## Create the test target, linking to the library
##
set(TEST_SOURCE_FILES test/tests.c test/cheat/cheat.h test/cheat/cheats.h test/civetweb/civetweb.c test/civetweb/md5.inl test/civetweb/handle_form.inl test/test_request_handlers.h test/test_request_handlers.c test/test_helpers.h test/test_helpers.c)
add_executable(runTests ${TEST_SOURCE_FILES})
target_link_libraries(runTests justapis-c-sdk -ldl -lm)#-ldl -lm is required e.g on Linux(Ubuntu 16.04 LTS)

##
## Enable a test target using CTEST that invokes the test target
##
enable_testing()
add_test(NAME test
         COMMAND runTests)

## Sample:

##
## Create the runable target, linking to the library
##
set(NS_MQTT_DEMO_CLI_SOURCE_FILES samples/ns_mqtt_demo_cli/src/main.c)
add_executable(ns-mqtt-demo-cli ${NS_MQTT_DEMO_CLI_SOURCE_FILES})
target_link_libraries(ns-mqtt-demo-cli justapis-c-sdk)