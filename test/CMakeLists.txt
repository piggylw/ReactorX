# 测试：监听标准输入
add_executable(test test.cpp)
target_link_libraries(test reactor pthread)