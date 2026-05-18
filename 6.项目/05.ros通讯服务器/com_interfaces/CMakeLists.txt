cmake_minimum_required(VERSION 3.8)

# 自动设置项目名
get_filename_component(CUR_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${CUR_DIR_NAME})

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)

# ------配置------
set(INTERFACE_DEPENDS
  geometry_msgs
)
# ------构建接口------
find_package(rosidl_default_generators REQUIRED)
foreach(INTERFACE_DEPEND ${INTERFACE_DEPENDS})
  find_package(${INTERFACE_DEPEND} REQUIRED)
endforeach()

file(GLOB_RECURSE MSG_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "msg/*.msg")
file(GLOB_RECURSE SRV_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "srv/*.srv")
file(GLOB_RECURSE ACTION_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "action/*.action")
if(MSG_FILES OR SRV_FILES OR ACTION_FILES)
  rosidl_generate_interfaces(${PROJECT_NAME} 
    ${MSG_FILES} ${SRV_FILES} ${ACTION_FILES} 
    DEPENDENCIES ${INTERFACE_DEPENDS})
endif()

ament_export_dependencies(rosidl_default_runtime)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
