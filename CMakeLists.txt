cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(redis-3.0 VERSION 3.0)

set(CMAKE_BUILD_TYPE "Debug")

message(STATUS "Host is: ${CMAKE_HOST_SYSTEM}.  Build target is: ${CMAKE_SYSTEM}")
get_filename_component(REDIS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
message(STATUS "Project root directory is: ${REDIS_ROOT}")

set(CMAKE_LEGACY_CYGWIN_WIN32 0)

add_subdirectory(deps)

set(SRC_SERVER
    src/adlist.c
    src/ae.c
    src/anet.c
    src/aof.c
    src/bio.c
    src/bitops.c
    src/blocked.c
    src/cluster.c
    src/config.c
    src/crc16.c
    src/crc64.c
    src/db.c
    src/debug.c
    src/dict.c
    src/endianconv.c
    src/hyperloglog.c
    src/intset.c
    src/latency.c
    src/lzf_c.c
    src/lzf_d.c
    src/memtest.c
    src/multi.c
    src/networking.c
    src/notify.c
    src/object.c
    src/pqsort.c
    src/pubsub.c
    src/rdb.c
    src/rand.c
    src/redis.c
    src/release.c
    src/replication.c
    src/rio.c
    src/scripting.c
    src/sds.c
    src/sentinel.c
    src/setproctitle.c
    src/sha1.c
    src/slowlog.c
    src/sort.c
    src/sparkline.c
    src/syncio.c
    src/t_hash.c
    src/t_list.c
    src/t_set.c
    src/t_string.c
    src/t_zset.c
    src/util.c
    src/zipmap.c
    src/ziplist.c
    src/zmalloc.c)

set(SRC_CLI
    src/adlist.c
    src/ae.c
    src/anet.c
    src/crc64.c
    src/redis-cli.c
    src/release.c
    src/sds.c
    src/zmalloc.c)


set(EXECUTABLE_OUTPUT_PATH src)
link_directories(deps/linenoise/ deps/lua/src deps/hiredis)

add_executable(redis-server ${SRC_SERVER})
target_include_directories(redis-server
                           PRIVATE ${REDIS_ROOT}/deps/linenoise
                           PRIVATE ${REDIS_ROOT}/deps/hiredis
                           PRIVATE ${REDIS_ROOT}/deps/lua/src)
target_link_libraries(redis-server
                      PRIVATE pthread
                      PRIVATE m
                      PRIVATE lua
                      PRIVATE linenoise
                      PRIVATE hiredis)


add_executable(redis-cli ${SRC_CLI})
target_include_directories(redis-cli
                           PRIVATE ${REDIS_ROOT}/deps/linenoise
                           PRIVATE ${REDIS_ROOT}/deps/hiredis
                           PRIVATE ${REDIS_ROOT}/deps/lua/src)
target_link_libraries(redis-cli
                      PRIVATE pthread
                      PRIVATE m
                      PRIVATE linenoise
                      PRIVATE hiredis)

add_subdirectory(test)
