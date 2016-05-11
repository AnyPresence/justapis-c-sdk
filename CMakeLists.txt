cmake_minimum_required(VERSION 3.5)
project(justapis-c-sdk)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")

## Make sure we have necessary dependencies installed
FIND_PACKAGE(CURL)
IF(CURL_FOUND)
    INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIR})
    SET(requiredlibs ${requiredlibs} ${CURL_LIBRARIES} )
ELSE(CURL_FOUND)
    MESSAGE(FATAL_ERROR "Could not find the CURL library and development files.")
ENDIF(CURL_FOUND)

##
## Create the library target
##
set(LIB_SOURCE_FILES src/justapis.c include/justapis.h include/cJSON.h src/cJSON.c)
add_library(justapis-c-sdk ${LIB_SOURCE_FILES})
target_link_libraries(justapis-c-sdk ${CURL_LIBRARIES})
target_include_directories(justapis-c-sdk PUBLIC include)

##
## Create the test target, linking to the library
##
set(TEST_SOURCE_FILES test/tests.c test/cheat/cheat.h test/cheat/cheats.h test/civetweb/civetweb.c test/civetweb/md5.inl test/civetweb/handle_form.inl test/test_request_handlers.h test/test_request_handlers.c test/test_helpers.h test/test_helpers.c)
add_executable(runTests ${TEST_SOURCE_FILES})
target_link_libraries(runTests justapis-c-sdk)

##
## Enable a test target using CTEST that invokes the test target
##
enable_testing()
add_test(NAME test
         COMMAND runTests)