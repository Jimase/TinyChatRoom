cmake_minimum_required(VERSION 3.5)
project(ServerClient)

set(CMAKE_CXX_STANDARD 11)

# 生成Server可执行文件
add_executable(Server Server.cpp)
target_link_libraries(Server pthread)

# 生成Client可执行文件
add_executable(Client Client.cpp)
target_link_libraries(Client pthread)

# 定义clean目标
add_custom_target(clean
        COMMAND ${CMAKE_COMMAND} -E remove Server Client
        )
