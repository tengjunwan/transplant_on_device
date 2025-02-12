# lvgl all source 

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thridpart/libui/lvgl)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/thridpart/libui/lv_drivers/display  DIR_LITTLE_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/thridpart/libui/app  DIR_LITTLE_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/thridpart/libui/qrcode  DIR_LITTLE_SRC)

INCLUDE_DIRECTORIES(
	${CMAKE_CURRENT_SOURCE_DIR}/thridpart/libui/
	${CMAKE_CURRENT_SOURCE_DIR}/thridpart/libui/lvgl/
	${CMAKE_CURRENT_SOURCE_DIR}/thridpart/libui/lv_drivers/
	${CMAKE_CURRENT_SOURCE_DIR}/thridpart/libui/app/
	${CMAKE_CURRENT_SOURCE_DIR}/thridpart/libui/qrcode/
)