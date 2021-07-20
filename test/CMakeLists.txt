add_executable(sds_test framework/test.c structure/sds_test.c)
target_include_directories(sds_test
                           PRIVATE structure
                           PRIVATE framework
                           PRIVATE ${REDIS_ROOT}/deps/linenoise
                           PRIVATE ${REDIS_ROOT}/deps/hiredis
                           PRIVATE ${REDIS_ROOT}/deps/lua/src)
target_link_libraries(sds_test
                      PRIVATE pthread
                      PRIVATE m
                      PRIVATE linenoise
                      PRIVATE hiredis)

