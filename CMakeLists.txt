cmake_minimum_required(VERSION 3.5)
PROJECT(ss928_yolov8)

include(${CMAKE_SOURCE_DIR}/cmake/ss928.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/cmakebase.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/project.cmake)

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${CMAKE_SOURCE_DIR}/output/")

INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/ss928sdk/common/
    ${CMAKE_SOURCE_DIR}/ss928sdk/common/audio/adp
    ${CMAKE_SOURCE_DIR}/ss928sdk/include/
    ${CMAKE_SOURCE_DIR}/ss928sdk/include/npu
    ${CMAKE_SOURCE_DIR}/ss928sdk/include/svp_npu
    ${CMAKE_SOURCE_DIR}/svp/
    ${CMAKE_SOURCE_DIR}/svp/common/
    ${CMAKE_SOURCE_DIR}/svp/npu/include/
    ${CMAKE_SOURCE_DIR}/svp/npu/sample_svp_npu/
    ${CMAKE_SOURCE_DIR}/thridpart/ncnn/include
    ${CMAKE_SOURCE_DIR}/thridpart/eigen3
    ${CMAKE_SOURCE_DIR}/svp/tracker/deepsort/include
    ${CMAKE_SOURCE_DIR}/svp/tracker/inc 
)
LINK_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/ss928sdk/lib/
    ${CMAKE_SOURCE_DIR}/ss928sdk/lib/npu/
    ${CMAKE_SOURCE_DIR}/ss928sdk/lib/svp_npu
    ${CMAKE_SOURCE_DIR}/thridpart/ncnn/lib
)

include(${CMAKE_SOURCE_DIR}/thridpart/libui/libui.cmake)
aux_source_directory(${CMAKE_SOURCE_DIR}/ss928sdk/common/ SOCSDKCOMMON_SRCLIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/ss928sdk/common/audio SOCSDKCOMMON_SRCLIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/svp/common/ SVP_SRCLIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/svp/npu/sample_svp_npu SVP_SRCLIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/svp/npu/ SVP_SRCLIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/svp/tracker/deepsort/src SVP_SRCLIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/svp/ SVP_SRCLIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/sample/ SVP_SRCLIST)

add_executable(${PROJECT_NAME} ${SVP_SRCLIST} ${SOCSDKCOMMON_SRCLIST} ${DIR_LITTLE_SRC})

target_link_libraries(${PROJECT_NAME} ${SOC_LIBS} ${SYSTEM_LINK_LIB} libncnn.a lvgl)


set(OUTPUT_LIB_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/lib)
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_LIB_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/source_file
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/doc/source_file/3840x2160_8bit.h264 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/source_file
    # COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/doc/atc/yolov5s_v6.2.om ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/doc/atc/yolov8n.om ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/doc/rundemo.sh ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/doc/atc/feature.om ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    # COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/doc/test.jpg ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    # COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/doc/640x640.jpg ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/ss928sdk/lib ${OUTPUT_LIB_DIR}
    COMMENT "Copying lib files to output directory"
)
add_custom_target(cpfiles ALL DEPENDS  ${PROJECT_NAME})
