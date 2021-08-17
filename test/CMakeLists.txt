# sds test
add_executable(sds_test ../src/sds.c ../src/zmalloc.c framework/test.c structure/sds_test.c)
target_include_directories(sds_test PRIVATE structure PRIVATE framework PRIVATE ${REDIS_ROOT}/src)

# dict test
add_executable(dict_test ../src/sds.c ../src/dict.c ../src/zmalloc.c framework/test.c structure/dict_test.c)
target_include_directories(dict_test PRIVATE structure PRIVATE framework PRIVATE ${REDIS_ROOT}/src)
