cmake_minimum_required(VERSION 3.10)
project(MYMUDUO)

# mymuduo最后编译得到.so动态库，设置动态库路径，为根目录下的lib文件夹下
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
# 设置调试信息并设置C++11标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

# 定义参与编译的源文件，当前目录下所有源文件
aux_source_directory(. SRC_LIST)
# 编译生成动态库mymuduo
add_library(mymuduo SHARED ${SRC_LIST})
