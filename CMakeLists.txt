cmake_minimum_required(VERSION 3.15)
project(nrpc_cpp VERSION 1.0.1)

###################
# Common settings #
###################
    string(ASCII 27 Esc)
    set(Reset "${Esc}[m")
    set(Yellow      "${Esc}[33m")
    message(STATUS "${Yellow}Common settings${Reset}")
    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
    set(Bold  "${Esc}[1m")
    set(Red         "${Esc}[31m")
    set(Green       "${Esc}[32m")
    set(Blue        "${Esc}[34m")
    set(Magenta     "${Esc}[35m")
    set(Cyan        "${Esc}[36m")
    set(White       "${Esc}[37m")
    cmake_policy(SET CMP0169 OLD)

######################
# Fetch dependencies #
######################
    message(STATUS "${Yellow}Fetch dependencies${Reset}")
    enable_language(CXX)
    enable_language(C)
    include(FetchContent)
    include(CMakePackageConfigHelpers)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY "https://github.com/nlohmann/json"
        GIT_TAG "v3.11.3" 
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/staging_modules/nlohmann_json"
        EXCLUDE_FROM_ALL
    )
    FetchContent_Declare(libzmq
        GIT_REPOSITORY "https://github.com/zeromq/libzmq.git"
        GIT_TAG "v4.3.5" 
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/staging_modules/libzmq"
        EXCLUDE_FROM_ALL
    )
    set(BOOST_INCLUDE_LIBRARIES regex algorithm beast mpl)
    set(BOOST_ENABLE_CMAKE ON)
    FetchContent_Declare(
      boost_content
      URL https://github.com/boostorg/boost/releases/download/boost-1.87.0/boost-1.87.0-cmake.tar.gz
      USES_TERMINAL_DOWNLOAD TRUE
      DOWNLOAD_NO_EXTRACT FALSE
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
      SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/staging_modules/boost"
      EXCLUDE_FROM_ALL
    )
    FetchContent_GetProperties(nlohmann_json)
    FetchContent_GetProperties(libzmq)
    FetchContent_GetProperties(boost_content)
    if(NOT nlohmann_json_POPULATED)
        FetchContent_Populate(nlohmann_json)
    endif()
    if(NOT libzmq_POPULATED)
        FetchContent_Populate(libzmq)
    endif()
    if(NOT boost_content_POPULATED)
        FetchContent_Populate(boost_content)
    endif()

######################
# Build dependency 1 #
######################
    message(STATUS "${Yellow}Build dependency 1${Reset}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -B build -Wno-deprecated  -Wno-dev -DJSON_BuildTests=OFF
        WORKING_DIRECTORY  ${CMAKE_CURRENT_LIST_DIR}/staging_modules/nlohmann_json
    )

######################
# Build dependency 2 #
######################
    message(STATUS "${Yellow}Build dependency 2${Reset}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build build -j20
        WORKING_DIRECTORY  ${CMAKE_CURRENT_LIST_DIR}/staging_modules/nlohmann_json
    )

######################
# Build dependency 3 #
######################
    message(STATUS "${Yellow}Build dependency 3${Reset}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -B build -Wno-deprecated  -Wno-dev -DWITH_TLS=OFF -DENABLE_DRAFTS=ON
        WORKING_DIRECTORY  ${CMAKE_CURRENT_LIST_DIR}/staging_modules/libzmq
    )

######################
# Build dependency 4 #
######################
    message(STATUS "${Yellow}Build dependency 4${Reset}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build build -j20
        WORKING_DIRECTORY  ${CMAKE_CURRENT_LIST_DIR}/staging_modules/libzmq
    )

######################
# Build dependency 5 #
######################
    message(STATUS "${Yellow}Build dependency 5${Reset}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -B build -Wno-deprecated  -Wno-dev -DCMAKE_INSTALL_MESSAGE=NEVER -DBUILD_SHARED_LIBS=OFF -DBoost_USE_STATIC_LIBS=ON -DBOOST_ENABLE_CMAKE=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_LIST_DIR}/staging_modules/boost_install
        WORKING_DIRECTORY  ${CMAKE_CURRENT_LIST_DIR}/staging_modules/boost
    )
        
######################
# Build dependency 6 #
######################
    message(STATUS "${Yellow}Build dependency 6${Reset}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build build -j20  --config Release
        WORKING_DIRECTORY  ${CMAKE_CURRENT_LIST_DIR}/staging_modules/boost
    )

######################
# Build dependency 7 #
######################
    message(STATUS "${Yellow}Build dependency 7${Reset}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --install build
        WORKING_DIRECTORY  ${CMAKE_CURRENT_LIST_DIR}/staging_modules/boost
    )

#####################
# Find dependencies #
#####################
    message(STATUS "${Yellow}Find dependencies${Reset}")
    set(boost_DIR ${CMAKE_CURRENT_LIST_DIR}/staging_modules/boost_install)
    cmake_policy(SET CMP0167 NEW)
    find_package(nlohmann_json REQUIRED HINTS "staging_modules/nlohmann_json/build")
    find_package(ZeroMQ REQUIRED HINTS "staging_modules/libzmq/build")
    find_package(Boost 1.87.0 REQUIRED COMPONENTS algorithm
                 HINTS "staging_modules/boost_install/lib/cmake")
    if(NOT "${nlohmann_json_DIR}" MATCHES "${CMAKE_CURRENT_LIST_DIR}/staging_modules/.*")
        message(FATAL_ERROR "Assertion failed: nlohmann_json_DIR is not valid")
    endif()
    if(NOT "${ZeroMQ_DIR}" MATCHES "${CMAKE_CURRENT_LIST_DIR}/staging_modules/.*")
        message(FATAL_ERROR "Assertion failed: ZeroMQ_DIR is not valid")
    endif()
    if(NOT "${boost_DIR}" MATCHES "${CMAKE_CURRENT_LIST_DIR}/staging_modules/.*")
        message(FATAL_ERROR "Assertion failed: boost_DIR is not valid")
    endif()
    if(NOT "${Boost_DIR}" MATCHES "${CMAKE_CURRENT_LIST_DIR}/staging_modules/.*")
        message(FATAL_ERROR "Assertion failed: Boost_DIR is not valid")
    endif()
    get_directory_property(DepsTargets IMPORTED_TARGETS)
    message(STATUS "Deps: ${DepsTargets}")
    message(STATUS "NlohmannJson Dir: ${nlohmann_json_DIR}")
    message(STATUS "ZeroMQ Dir: ${ZeroMQ_DIR}")
    message(STATUS "ZeroMQ Include: ${ZeroMQ_INCLUDE_DIR}")
    message(STATUS "ZeroMQ Lib: ${ZeroMQ_STATIC_LIBRARY}")

#################
# Setup targets #
#################
    message(STATUS "${Yellow}Setup targets${Reset}")
    list(
        APPEND NrpcCppSources
        src/client_socket.cpp
        src/common_base.cpp
        src/nrpc_cpp.cpp
        src/routing_socket.cpp
        src/server_socket.cpp
        src/service_client.cpp
    )
    list(
        APPEND NrpcCppInclude
        src/nrpc_cpp.hpp
        src/common_base.hpp
        src/client_socket.hpp
        src/routing_socket.hpp
        src/server_socket.hpp
        src/service_client.hpp
    )
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
    add_library(nrpc_cpp STATIC ${NrpcCppSources})
    add_executable(test_schema test/test_schema.cpp)
    add_executable(test_show test/test_show.cpp)
    add_executable(test_show_client test/test_show_client.cpp)
    add_executable(test_json test/test_json.cpp)
    add_executable(test_zmq test/test_zmq.cpp)
    target_link_libraries(nrpc_cpp nlohmann_json::nlohmann_json libzmq-static Boost::algorithm)
    target_link_libraries(test_schema nlohmann_json::nlohmann_json libzmq-static nrpc_cpp)
    target_link_libraries(test_show nlohmann_json::nlohmann_json libzmq-static nrpc_cpp)
    target_link_libraries(test_show_client nlohmann_json::nlohmann_json libzmq-static nrpc_cpp)
    target_link_libraries(test_json nlohmann_json::nlohmann_json libzmq-static nrpc_cpp)
    target_link_libraries(test_zmq nlohmann_json::nlohmann_json libzmq-static nrpc_cpp)

#################
# Setup exports #
#################
    message(STATUS "${Yellow}Setup exports${Reset}")
    get_directory_property(FinalTargets BUILDSYSTEM_TARGETS)
    message(STATUS "Export: ${FinalTargets}")
    file(COPY ${NrpcCppInclude} DESTINATION "${CMAKE_BINARY_DIR}/include/nrpc_cpp")
    export(TARGETS nrpc_cpp test_json test_zmq NAMESPACE nrpc_cpp:: FILE "${CMAKE_CURRENT_BINARY_DIR}/nrpc_cppTargets.cmake")
    include(CMakePackageConfigHelpers)

    # Template:
    #   @PACKAGE_INIT@
    #   include("${CMAKE_CURRENT_LIST_DIR}/nrpc_cppTargets.cmake")
    #
    file(WRITE 
        "${CMAKE_CURRENT_BINARY_DIR}/nrpc_cppConfig.cmake"
        "get_filename_component(PACKAGE_PREFIX_DIR \"\${CMAKE_CURRENT_LIST_DIR}/../\" ABSOLUTE)\n"
        "\n"
        "macro(set_and_check _var _file)\n"
        "  set(\${_var} \"\${_file}\")\n"
        "  if(NOT EXISTS \"\${_file}\")\n"
        "    message(FATAL_ERROR \"File or directory \${_file} referenced by variable \${_var} does not exist !\")\n"
        "  endif()\n"
        "endmacro()\n"
        "\n"
        "macro(check_required_components _NAME)\n"
        "  foreach(comp \${\${_NAME}_FIND_COMPONENTS})\n"
        "    if(NOT \${_NAME}_\${comp}_FOUND)\n"
        "      if(\${_NAME}_FIND_REQUIRED_\${comp})\n"
        "        set(\${_NAME}_FOUND FALSE)\n"
        "      endif()\n"
        "    endif()\n"
        "  endforeach()\n"
        "endmacro()\n"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/nrpc_cppTargets.cmake\")\n"
        "set(nrpc_cpp_INCLUDE_DIR \"\${nrpc_cpp_DIR}/include\")\n"
    )

    # configure_package_config_file(
    #     nrpc_cppConfig.cmake.in
    #     "${CMAKE_CURRENT_BINARY_DIR}/nrpc_cppConfig.cmake"
    #     INSTALL_DESTINATION CMake
    # )
