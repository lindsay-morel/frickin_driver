cmake_minimum_required(VERSION 3.10)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(memx_jni LANGUAGES CXX)
endif()

set(CMAKE_BUILD_TYPE Debug)

# ----------------------------
# Platform-Specific JAVA_HOME
# ----------------------------
if(WIN32)
    message(STATUS "Configuring for Windows platform")

    if(NOT DEFINED JAVA_HOME_PATH)
        set(JAVA_HOME_PATH "$ENV{JAVA_HOME}")
        if(NOT EXISTS "${JAVA_HOME_PATH}")
            message(FATAL_ERROR "JAVA_HOME not set or invalid. Please set JAVA_HOME_PATH manually.")
        endif()
    endif()

    set(JAVA_HOME "${JAVA_HOME_PATH}")

    include_directories(
        "${JAVA_HOME}/include"
        "${JAVA_HOME}/include/win32"
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    add_compile_definitions(
        JINT_PTR\(ptr\)=reinterpret_cast\<const jint*\>\(ptr\)
        JSIZE_CAST\(x\)=static_cast\<jsize\>\(x\)
        MAYBE_UNUSED=
    )

elseif(UNIX)
    message(STATUS "Configuring for Unix/Linux platform")

    if(NOT DEFINED JAVA_HOME_PATH)
        message(STATUS "JAVA_HOME_PATH is not predefined, detecting by architecture: ${CMAKE_SYSTEM_PROCESSOR}")
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(JAVA_HOME_PATH "/usr/lib/jvm/java-17-openjdk-arm64")
            message(STATUS "Detected ARM64 architecture, setting JAVA_HOME_PATH to: ${JAVA_HOME_PATH}")

        elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
            set(JAVA_HOME_PATH "/usr/lib/jvm/java-17-openjdk-amd64")
            message(STATUS "Detected x86_64 architecture, setting JAVA_HOME_PATH to: ${JAVA_HOME_PATH}")

        else()
            message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}. Set JAVA_HOME_PATH manually.")
        endif()
    endif()

    if(EXISTS "${JAVA_HOME_PATH}")
        set(JAVA_HOME "${JAVA_HOME_PATH}")
    else()
        message(WARNING "JAVA_HOME_PATH does not exist: ${JAVA_HOME_PATH}")
        set(JAVA_HOME "")
    endif()

    include_directories(
        ${JAVA_HOME}/include
        ${JAVA_HOME}/include/linux
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        /usr/include/memx
    )

    add_compile_definitions(
        JINT_PTR\(ptr\)=reinterpret_cast\<const jint*\>\(ptr\)
        JSIZE_CAST\(x\)=static_cast\<jsize\>\(x\)
        MAYBE_UNUSED="[[maybe_unused]]"
    )

endif()

# ----------------------------
# Find Java
# ----------------------------
find_package(Java REQUIRED COMPONENTS Development Runtime)
message(STATUS "Found Java Compiler: ${Java_JAVAC_EXECUTABLE}")
message(STATUS "Found Java Runtime: ${Java_JAVA_EXECUTABLE}")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ----------------------------
# Build Shared Library
# ----------------------------
add_library(${CMAKE_PROJECT_NAME} SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/memx_jni.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dfp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mxpack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/convert.cpp
)

# ----------------------------
# Set Output Directory
# ----------------------------
if(NOT DEFINED JNI_OUTPUT_DIR)
    set(JNI_OUTPUT_DIR ${CMAKE_BINARY_DIR})
endif()

foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${config} ${JNI_OUTPUT_DIR}
        ARCHIVE_OUTPUT_DIRECTORY_${config} ${JNI_OUTPUT_DIR}
        RUNTIME_OUTPUT_DIRECTORY_${config} ${JNI_OUTPUT_DIR}
    )
endforeach()

# ----------------------------
# Link Libraries
# ----------------------------
if(WIN32)
    find_library(UDRIVER_LIB
        NAMES udriver
        PATHS "${CMAKE_CURRENT_SOURCE_DIR}"
        NO_DEFAULT_PATH
    )
    target_link_libraries(${CMAKE_PROJECT_NAME}
        PRIVATE
        ${UDRIVER_LIB}
    )
    add_custom_command(TARGET ${CMAKE_PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/udriver.dll"
          "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>"
    )

elseif(UNIX)
    target_link_libraries(${CMAKE_PROJECT_NAME}
        memx
        atomic
        m
    )
endif()
