cmake_minimum_required(VERSION 3.10)
project(SmartHomeBetaTest)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(PROJECT_ROOT ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)
message(STATUS "Project root: ${PROJECT_ROOT}")

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(JSONCPP QUIET jsoncpp)
endif()

# 自动收集所有 .cpp 文件
file(GLOB SRCS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_executable(Server ${SRCS})

target_include_directories(Server PRIVATE
  ${PROJECT_ROOT}
  ${SQLite3_INCLUDE_DIRS}
)
target_link_libraries(Server PRIVATE
  Threads::Threads
  SQLite::SQLite3
)


# ========================================= JsonCpp链接库 ============================================
if(JSONCPP_FOUND)
  message(STATUS "JsonCpp via pkg-config: inc=${JSONCPP_INCLUDE_DIRS} lib=${JSONCPP_LIBRARIES}")
  target_include_directories(Server PRIVATE ${JSONCPP_INCLUDE_DIRS})
  target_link_libraries(Server PRIVATE ${JSONCPP_LIBRARIES})
else()
  find_path(JSONCPP_INCLUDE_DIR jsoncpp/json/json.h PATH_SUFFIXES jsoncpp)
  find_library(JSONCPP_LIB NAMES jsoncpp JsonCpp)
  if(JSONCPP_INCLUDE_DIR)
    target_include_directories(Server PRIVATE ${JSONCPP_INCLUDE_DIR}/..)
  endif()
  if(JSONCPP_LIB)
    target_link_libraries(Server PRIVATE ${JSONCPP_LIB})
    message(STATUS "JsonCpp found: ${JSONCPP_LIB}")
  else()
    message(FATAL_ERROR "JsonCpp not found. 请安装: sudo apt-get install -y libjsoncpp-dev")
  endif()
endif()


# ============================================ SeetaFace2人脸识别库 ========================================
option(SEETA_ENABLED "Enable SeetaFace2 engine" ON)
set(SEETA_ROOT "/home/crystalchen/SeetaFace2/install" CACHE PATH "SeetaFace2 install prefix")
message(STATUS "SEETA_ENABLED=${SEETA_ENABLED} SEETA_ROOT=${SEETA_ROOT}")

if(SEETA_ENABLED)
  target_compile_definitions(Server PRIVATE SEETA_ENABLED)
  target_include_directories(Server PRIVATE ${SEETA_ROOT}/include)

  set(SEETA_LIB_DIRS ${SEETA_ROOT}/lib ${SEETA_ROOT}/lib64)
  find_library(SEETA_FD  NAMES SeetaFaceDetector    HINTS ${SEETA_LIB_DIRS})
  find_library(SEETA_FA  NAMES SeetaFaceLandmarker  HINTS ${SEETA_LIB_DIRS})
  find_library(SEETA_FR  NAMES SeetaFaceRecognizer  HINTS ${SEETA_LIB_DIRS})
  find_library(SEETA_NET NAMES SeetaNet             HINTS ${SEETA_LIB_DIRS})
  find_library(SEETA_TRK NAMES SeetaFaceTracker     HINTS ${SEETA_LIB_DIRS})
  find_library(SEETA_QA  NAMES SeetaQualityAssessor HINTS ${SEETA_LIB_DIRS})

  message(STATUS "Found Seeta FD: ${SEETA_FD}")
  message(STATUS "Found Seeta FA: ${SEETA_FA}")
  message(STATUS "Found Seeta FR: ${SEETA_FR}")
  message(STATUS "Found Seeta Net: ${SEETA_NET}")

  if(NOT SEETA_FD OR NOT SEETA_FA OR NOT SEETA_FR OR NOT SEETA_NET)
    message(FATAL_ERROR "Seeta libs not found. 检查 SEETA_ROOT(${SEETA_ROOT})/lib 或 lib64")
  endif()

  target_link_libraries(Server PRIVATE
    ${SEETA_FD} ${SEETA_FA} ${SEETA_FR} ${SEETA_NET}
  )
  if(SEETA_TRK)
    target_link_libraries(Server PRIVATE ${SEETA_TRK})
  endif()
  if(SEETA_QA)
    target_link_libraries(Server PRIVATE ${SEETA_QA})
  endif()

  #=============================================== OpenCV库 ====================================================
  find_package(OpenCV QUIET COMPONENTS imgcodecs imgproc)
  if(OpenCV_FOUND)
    target_include_directories(Server PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(Server PRIVATE ${OpenCV_LIBS})
    message(STATUS "OpenCV found: ${OpenCV_VERSION}")
  endif()
endif()


# ============================================ libevent链接库 ===================================================
if(PKG_CONFIG_FOUND)
  pkg_check_modules(LIBEVENT QUIET libevent)
endif()
if(LIBEVENT_FOUND)
  message(STATUS "libevent via pkg-config: inc=${LIBEVENT_INCLUDE_DIRS} lib=${LIBEVENT_LIBRARIES}")
  target_include_directories(Server PRIVATE ${LIBEVENT_INCLUDE_DIRS})
  target_link_libraries(Server PRIVATE ${LIBEVENT_LIBRARIES})
else()
  find_path(LIBEVENT_INCLUDE_DIR event2/event.h)
  if(LIBEVENT_INCLUDE_DIR)
    target_include_directories(Server PRIVATE ${LIBEVENT_INCLUDE_DIR})
  endif()
  find_library(LIBEVENT           NAMES event)
  find_library(LIBEVENT_CORE      NAMES event_core)
  find_library(LIBEVENT_EXTRA     NAMES event_extra)
  find_library(LIBEVENT_PTHREADS  NAMES event_pthreads)
  target_link_libraries(Server PRIVATE
    ${LIBEVENT} ${LIBEVENT_CORE} ${LIBEVENT_EXTRA} ${LIBEVENT_PTHREADS}
  )
endif()


# ========================================== mosquitto链接库 ===================================================
if(PKG_CONFIG_FOUND)
  pkg_check_modules(MOSQ QUIET libmosquitto)
endif()
if(MOSQ_FOUND)
  message(STATUS "mosquitto via pkg-config: inc=${MOSQ_INCLUDE_DIRS} lib=${MOSQ_LIBRARIES}")
  target_include_directories(Server PRIVATE ${MOSQ_INCLUDE_DIRS})
  target_link_libraries(Server PRIVATE ${MOSQ_LIBRARIES})
else()
  find_path(MOSQ_INCLUDE_DIR mosquitto.h)
  if(MOSQ_INCLUDE_DIR)
    target_include_directories(Server PRIVATE ${MOSQ_INCLUDE_DIR})
  endif()
  find_library(MOSQ_LIB NAMES mosquitto)
  if(MOSQ_LIB)
    target_link_libraries(Server PRIVATE ${MOSQ_LIB})
  else()
    message(FATAL_ERROR "libmosquitto not found. 请安装: sudo apt-get install -y libmosquitto-dev")
  endif()
endif()


#========================================== Jsoncpp 头前缀宏 =====================================================
if(JSONCPP_INCLUDE_DIRS)
  get_filename_component(JSONCPP_ROOT_DIR ${JSONCPP_INCLUDE_DIRS} DIRECTORY)
  if(EXISTS "${JSONCPP_ROOT_DIR}/jsoncpp/json/json.h")
    target_compile_definitions(Server PRIVATE -DJSONCPP_INCLUDE_PREFIX)
  endif()
endif()
