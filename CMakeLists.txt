cmake_minimum_required(VERSION 3.10)

find_package(PkgConfig)
pkg_check_modules(libusb REQUIRED libusb-1.0)
pkg_check_modules(libmtp REQUIRED libmtp)

project(dtptd)

file(GLOB SOURCE_FILES "source/*.c")
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_link_directories(${PROJECT_NAME} PUBLIC ${libusb_LIBRARY_DIRS} ${libmtp_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} ${libusb_LIBRARIES} ${libmtp_LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC "include" ${libusb_INCLUDE_DIRS} ${libmtp_INCLUDE_DIRS})
