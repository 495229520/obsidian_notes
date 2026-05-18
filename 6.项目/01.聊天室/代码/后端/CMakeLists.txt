# src/CMakeLists.txt

set(SOURCES
    main.cpp
    ChatServer.cpp
    EpollServer.cpp
)

add_executable(chatservice ${SOURCES})

# 让头文件可以用 #include "xxx.hpp"
target_include_directories(chatservice
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Linux 上默认就会链接 pthread 和系统库，这里一般不需要额外设置
# 如果遇到找不到某些符号，可以再加：
# target_link_libraries(chatservice PRIVATE pthread)
