cmake_minimum_required(VERSION 3.10)
project(timingTask VERSION 2.0.0 LANGUAGES C)

#===========================================================================
# 依赖路径
#===========================================================================
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(LINKEDLIST_ROOT ${PROJECT_ROOT}/linkedList)
set(LINKEDLIST_INC  ${LINKEDLIST_ROOT}/include)
set(LINKEDLIST_LIB  ${LINKEDLIST_ROOT}/build/liblinkedlist.a)

set(THREADPOOL_ROOT ${PROJECT_ROOT}/threadPool)
set(THREADPOOL_INC  ${THREADPOOL_ROOT}/include)
set(THREADPOOL_LIB  ${THREADPOOL_ROOT}/build/libthreadpool.a)

#===========================================================================
# 库: libtiming_task (静态库，生产环境)
#===========================================================================
add_library(timing_task STATIC
    src/timing_task.c
)

target_include_directories(timing_task PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LINKEDLIST_INC}
    ${THREADPOOL_INC}
)

target_link_libraries(timing_task PUBLIC
    ${LINKEDLIST_LIB}
    ${THREADPOOL_LIB}
)

target_compile_options(timing_task PRIVATE
    -Wall -Wextra -Werror
    -Wdeclaration-after-statement
    -O2 -std=gnu11
)

#===========================================================================
# 示例: demo
#===========================================================================
add_executable(demo
    examples/demo.c
)
target_link_libraries(demo PRIVATE timing_task)

#===========================================================================
# 单元测试
#
# 测试源文件
#===========================================================================
set(TEST_SOURCES
    src/timing_task.c
    tests/runner.c
    tests/test_task.c
)

add_executable(test_runner ${TEST_SOURCES})

target_include_directories(test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LINKEDLIST_INC}
    ${THREADPOOL_INC}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(test_runner PRIVATE
    ${LINKEDLIST_LIB}
    ${THREADPOOL_LIB}
    pthread
)

# 测试使用 -O0 -g 方便调试，不加 -Werror（允许边界探索测试）
target_compile_options(test_runner PRIVATE
    -Wall -Wextra -O0 -g
)

#===========================================================================
# 安装规则
#===========================================================================
install(TARGETS timing_task
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/
    DESTINATION include
)
