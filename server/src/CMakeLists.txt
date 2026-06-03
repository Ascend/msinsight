set(SRC_HOME_DIR ${HOME_DIR}/src)

# usockets definitions
add_compile_definitions("LIBUS_NO_SSL")
add_compile_definitions("UWS_NO_ZLIB")
add_compile_definitions("LIBUS_USE_LIBUV")

include_directories(${SRC_HOME_DIR}/../msinsight/src)
include_directories(${SRC_HOME_DIR}/../msinsight/src/utils)
include_directories(${SRC_HOME_DIR}/../msinsight/src/base)

# src defs
include_directories(${SRC_HOME_DIR}/defs)

# src utils
include_directories(${SRC_HOME_DIR}/utils)
aux_source_directory(${SRC_HOME_DIR}/utils DIC_UTILS_SRC)

# src channel
include_directories(${SRC_HOME_DIR}/channel)
aux_source_directory(${SRC_HOME_DIR}/channel DIC_CHANNEL_SRC)

# src server
include_directories(${SRC_HOME_DIR}/server)
aux_source_directory(${SRC_HOME_DIR}/server DIC_SERVER_SRC)

# src protocol
include_directories(${SRC_HOME_DIR}/protocol)
aux_source_directory(${SRC_HOME_DIR}/protocol DIC_PROTOCOL_SRC)

# src modules
include_directories(${SRC_HOME_DIR}/modules)
include_directories(${SRC_HOME_DIR}/modules/base)
include_directories(${SRC_HOME_DIR}/modules/defs)
include_directories(${SRC_HOME_DIR}/modules/base/core)
include_directories(${SRC_HOME_DIR}/modules/base/utils)
include_directories(${SRC_HOME_DIR}/modules/base/handler)
include_directories(${SRC_HOME_DIR}/modules/base/protocol)
include_directories(${SRC_HOME_DIR}/modules/global)
include_directories(${SRC_HOME_DIR}/modules/global/core)
include_directories(${SRC_HOME_DIR}/modules/global/handler)
include_directories(${SRC_HOME_DIR}/modules/global/protocol)
include_directories(${SRC_HOME_DIR}/modules/global/database)
include_directories(${SRC_HOME_DIR}/modules/timeline)
include_directories(${SRC_HOME_DIR}/modules/timeline/core)
include_directories(${SRC_HOME_DIR}/modules/timeline/core/database)
include_directories(${SRC_HOME_DIR}/modules/timeline/core/database/helper)
include_directories(${SRC_HOME_DIR}/modules/timeline/core/analyze)
include_directories(${SRC_HOME_DIR}/modules/timeline/core/cache)
include_directories(${SRC_HOME_DIR}/modules/timeline/core/parser)
include_directories(${SRC_HOME_DIR}/modules/timeline/core/table)
include_directories(${SRC_HOME_DIR}/modules/timeline/core/dataEngine)
include_directories(${SRC_HOME_DIR}/modules/timeline/core/kernelE2E)
include_directories(${SRC_HOME_DIR}/modules/timeline/core/systemView)
include_directories(${SRC_HOME_DIR}/modules/timeline/handler)
include_directories(${SRC_HOME_DIR}/modules/timeline/protocol)
include_directories(${SRC_HOME_DIR}/modules/timeline/parser)
include_directories(${SRC_HOME_DIR}/modules/timeline/cache)
include_directories(${SRC_HOME_DIR}/modules/summary)
include_directories(${SRC_HOME_DIR}/modules/summary/core)
include_directories(${SRC_HOME_DIR}/modules/summary/protocol)
include_directories(${SRC_HOME_DIR}/modules/summary/handler)
include_directories(${SRC_HOME_DIR}/modules/summary/database)
include_directories(${SRC_HOME_DIR}/modules/summary/parser)
include_directories(${SRC_HOME_DIR}/modules/memory)
include_directories(${SRC_HOME_DIR}/modules/memory/database)
include_directories(${SRC_HOME_DIR}/modules/memory/handler)
include_directories(${SRC_HOME_DIR}/modules/memory/parser)
include_directories(${SRC_HOME_DIR}/modules/memory/protocol)
include_directories(${SRC_HOME_DIR}/modules/memory/table)
include_directories(${SRC_HOME_DIR}/modules/memory/service)
include_directories(${SRC_HOME_DIR}/modules/memory/view)
include_directories(${SRC_HOME_DIR}/modules/communication)
include_directories(${SRC_HOME_DIR}/modules/communication/database)
include_directories(${SRC_HOME_DIR}/modules/communication/protocol)
include_directories(${SRC_HOME_DIR}/modules/communication/handler)
include_directories(${SRC_HOME_DIR}/modules/communication/parser)
include_directories(${SRC_HOME_DIR}/modules/communication/core)
include_directories(${SRC_HOME_DIR}/modules/communication/advisor)
include_directories(${SRC_HOME_DIR}/modules/memscope)
include_directories(${SRC_HOME_DIR}/modules/memscope/database)
include_directories(${SRC_HOME_DIR}/modules/memscope/entity)
include_directories(${SRC_HOME_DIR}/modules/memscope/handler)
include_directories(${SRC_HOME_DIR}/modules/memscope/protocol)
include_directories(${SRC_HOME_DIR}/modules/memscope/service)
include_directories(${SRC_HOME_DIR}/modules/memscope/parser)
include_directories(${SRC_HOME_DIR}/modules/memsnapshot)
include_directories(${SRC_HOME_DIR}/modules/memsnapshot/database)
include_directories(${SRC_HOME_DIR}/modules/memsnapshot/handler)
include_directories(${SRC_HOME_DIR}/modules/memsnapshot/protocol)
include_directories(${SRC_HOME_DIR}/modules/memsnapshot/parser)
include_directories(${SRC_HOME_DIR}/modules/memsnapshot/service)
# Operator
include_directories(${SRC_HOME_DIR}/modules/operator)
include_directories(${SRC_HOME_DIR}/modules/operator/database)
include_directories(${SRC_HOME_DIR}/modules/operator/handler)
include_directories(${SRC_HOME_DIR}/modules/operator/parser)
include_directories(${SRC_HOME_DIR}/modules/operator/protocol)
# full db
include_directories(${SRC_HOME_DIR}/modules/full_db)
include_directories(${SRC_HOME_DIR}/modules/full_db/ProjectParse)
include_directories(${SRC_HOME_DIR}/modules/full_db/ProjectParse/ProjectChecker)
include_directories(${SRC_HOME_DIR}/modules/full_db/ParseUnit)
include_directories(${SRC_HOME_DIR}/modules/full_db/database)
# advisor
include_directories(${SRC_HOME_DIR}/modules/advisor)
include_directories(${SRC_HOME_DIR}/modules/advisor/handler)
include_directories(${SRC_HOME_DIR}/modules/advisor/protocol)
include_directories(${SRC_HOME_DIR}/modules/advisor/process)
aux_source_directory(${SRC_HOME_DIR}/modules/advisor DIC_MODULES_ADVISOR_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/advisor/handler DIC_MODULES_ADVISOR_HANDLER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/advisor/process DIC_MODULES_ADVISOR_PROCESS_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/advisor/protocol DIC_MODULES_ADVISOR_PROTOCOL_SRC)

# Reinforcement Learning 强化学习
include_directories(${SRC_HOME_DIR}/modules/rl)
include_directories(${SRC_HOME_DIR}/modules/rl/core)
include_directories(${SRC_HOME_DIR}/modules/rl/protocol)
include_directories(${SRC_HOME_DIR}/modules/rl/handler)

# servitization
include_directories(${SRC_HOME_DIR}/modules/servitization)
include_directories(${SRC_HOME_DIR}/modules/servitization/api)
include_directories(${SRC_HOME_DIR}/modules/servitization/context)
include_directories(${SRC_HOME_DIR}/modules/servitization/handler)
include_directories(${SRC_HOME_DIR}/modules/servitization/protocol)
include_directories(${SRC_HOME_DIR}/modules/servitization/repo)

# modules for server
aux_source_directory(${SRC_HOME_DIR}/modules DIC_MODULES_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/base DIC_MODULES_BASE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/base/core DIC_MODULES_BASE_CORE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/base/handler DIC_MODULES_BASE_HANDLER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/base/protocol DIC_MODULES_BASE_PROTOCOL_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/global DIC_MODULES_GLOBAL_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/global/core DIC_MODULES_GLOBAL_CORE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/global/handler DIC_MODULES_GLOBAL_HANDLER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/global/protocol DIC_MODULES_GLOBAL_PROTOCOL_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/global/database DIC_MODULES_GLOBAL_DATABASE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline  DIC_MODULES_TIMELINE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/core DIC_MODULES_TIMELINE_CORE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/core/database DIC_MODULES_TIMELINE_CORE_DATABASE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/core/database/helper DIC_MODULES_TIMELINE_CORE_DATABASE_HELPER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/core/analyze DIC_MODULES_TIMELINE_CORE_ANALYZE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/core/cache DIC_MODULES_TIMELINE_CORE_CACHE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/core/parser DIC_MODULES_TIMELINE_CORE_PARSER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/core/table DIC_MODULES_TIMELINE_CORE_TABLE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/core/dataEngine DIC_MODULES_TIMELINE_CORE_DATAENGINE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/core/kernelE2E DIC_MODULES_TIMELINE_CORE_KERNELE2E_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/core/systemView DIC_MODULES_TIMELINE_CORE_SYSTEMVIEW_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/handler DIC_MODULES_TIMELINE_HANDLER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/protocol DIC_MODULES_TIMELINE_PROTOCOL_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/parser DIC_MODULES_TIMELINE_PARSER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/timeline/cache DIC_MODULES_TIMELINE_CACHE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/summary DIC_MODULES_SUMMARY_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/summary/core DIC_MODULES_SUMMARY_CORE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/summary/handler DIC_MODULES_SUMMARY_HANDLER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/summary/protocol DIC_MODULES_SUMMARY_PROTOCOL_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/summary/database DIC_MODULES_SUMMARY_DATABASE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/summary/parser DIC_MODULES_SUMMARY_PARSER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memory  DIC_MODULES_MEMORY_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memory/database DIC_MODULES_MEMORY_DATABASE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memory/handler DIC_MODULES_MEMORY_HANDLER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memory/parser DIC_MODULES_MEMORY_PARSER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memory/protocol DIC_MODULES_MEMORY_PROTOCOL_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memory/table DIC_MODULES_MEMORY_TABLE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memory/service DIC_MODULES_MEMORY_SERVICE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memory/view DIC_MODULES_MEMORY_VIEW_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/communication/database DIC_MODULES_COMMUNICATION_DATABASE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/communication/protocol DIC_MODULES_COMMUNICATION_PROTOCOL_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/communication/handler DIC_MODULES_COMMUNICATION_HANDLER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/communication/parser DIC_MODULES_COMMUNICATION_PARSER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/communication DIC_MODULES_COMMUNICATION_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/communication/core DIC_MODULES_COMMUNICATION_CORE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/communication/advisor DIC_MODULES_COMMUNICATION_ADVISOR_SRC)
# Operator
aux_source_directory(${SRC_HOME_DIR}/modules/operator  DIC_MODULES_OPERATOR_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/operator/database DIC_MODULES_OPERATOR_DATABASE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/operator/handler DIC_MODULES_OPERATOR_HANDLER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/operator/parser DIC_MODULES_OPERATOR_PARSER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/operator/protocol DIC_MODULES_OPERATOR_PROTOCOL_SRC)

# Source
include_directories(${SRC_HOME_DIR}/modules/source)
include_directories(${SRC_HOME_DIR}/modules/source/handler)
include_directories(${SRC_HOME_DIR}/modules/source/parser)
include_directories(${SRC_HOME_DIR}/modules/source/protocol)
include_directories(${SRC_HOME_DIR}/modules/source/core)

# Source
aux_source_directory(${SRC_HOME_DIR}/modules/source  DIC_MODULES_SOURCE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/source/handler DIC_MODULES_SOURCE_HANDLER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/source/parser DIC_MODULES_SOURCE_PARSER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/source/protocol DIC_MODULES_SOURCE_PROTOCOL_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/source/core DIC_MODULES_SOURCE_CORE_SRC)

# full db
aux_source_directory(${SRC_HOME_DIR}/modules/full_db  DIC_MODULES_FULL_DB_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/full_db/ProjectParse  DIC_MODULES_FULL_DB_PARSER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/full_db/database  DIC_MODULES_FULL_DB_DATABASE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/full_db/ProjectParse/ProjectChecker DIC_MODULES_FULL_DB_PARSER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/full_db/ParseUnit DIC_MODULES_FULL_DB_PARSER_UNIT_SRC)

# IE
aux_source_directory(${SRC_HOME_DIR}/modules/servitization DIC_MODULES_IE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/servitization/api DIC_MODULES_IE_API_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/servitization/context DIC_MODULES_IE_CONTEXT_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/servitization/handler DIC_MODULES_IE_HANDLER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/servitization/protocol DIC_MODULES_IE_PROTOCOL_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/servitization/repo DIC_MODULES_IE_REPO_SRC)

# Memory Detail
aux_source_directory(${SRC_HOME_DIR}/modules/memscope DIC_MODULES_MEM_SCOPE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memscope/database DIC_MODULES_MEM_SCOPE_DB_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memscope/handler DIC_MODULES_MEM_SCOPE_HANDLER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memscope/service DIC_MODULES_MEM_SCOPE_SVC_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memscope/protocol DIC_MODULES_MEM_SCOPE_PROTOCOL_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memscope/entity DIC_MODULES_MEM_SCOPE_ENTITY_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memscope/parser DIC_MODULES_MEM_SCOPE_PARSER_SRC)

# Memory Snapshot
aux_source_directory(${SRC_HOME_DIR}/modules/memsnapshot DIC_MODULES_MEM_SNAPSHOT_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memsnapshot/database DIC_MODULES_MEM_SNAPSHOT_DB_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memsnapshot/handler DIC_MODULES_MEM_SNAPSHOT_HANDLER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memsnapshot/protocol DIC_MODULES_MEM_SNAPSHOT_PROTOCOL_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memsnapshot/parser DIC_MODULES_MEM_SNAPSHOT_PARSER_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/memsnapshot/service DIC_MODULES_MEM_SNAPSHOT_SVC_SRC)

# Reinforcement Learning
aux_source_directory(${SRC_HOME_DIR}/modules/rl DIC_MODULES_RL_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/rl/core DIC_MODULES_RL_CORE_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/rl/protocol DIC_MODULES_RL_PROTOCOL_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/rl/handler DIC_MODULES_RL_HANDLER_SRC)

# triton parser
aux_source_directory(${SRC_HOME_DIR}/modules/triton DIC_MODULES_TRITON_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/triton/parser DIC_MODULES_TRITON_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/triton/protocol DIC_MODULES_TRITON_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/triton/service DIC_MODULES_TRITON_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/triton/handler DIC_MODULES_TRITON_SRC)
aux_source_directory(${SRC_HOME_DIR}/modules/triton/allocator DIC_MODULES_TRITON_SRC)
include_directories(${SRC_HOME_DIR}/modules/triton)
include_directories(${SRC_HOME_DIR}/modules/triton/parser)
include_directories(${SRC_HOME_DIR}/modules/triton/protocol)
include_directories(${SRC_HOME_DIR}/modules/triton/service)
include_directories(${SRC_HOME_DIR}/modules/triton/handler)
include_directories(${SRC_HOME_DIR}/modules/triton/allocator)
include_directories(${SRC_HOME_DIR}/modules/triton/defs)
list(APPEND DIC_MODULES_SRC_LIST
        ${DIC_MODULES_SRC}
        ${DIC_MODULES_BASE_SRC}
        ${DIC_MODULES_BASE_CORE_SRC}
        ${DIC_MODULES_BASE_HANDLER_SRC}
        ${DIC_MODULES_BASE_PROTOCOL_SRC}
        ${DIC_MODULES_GLOBAL_SRC}
        ${DIC_MODULES_GLOBAL_CORE_SRC}
        ${DIC_MODULES_GLOBAL_HANDLER_SRC}
        ${DIC_MODULES_GLOBAL_PROTOCOL_SRC}
        ${DIC_MODULES_GLOBAL_DATABASE_SRC}
        ${DIC_MODULES_TIMELINE_SRC}
        ${DIC_MODULES_TIMELINE_CORE_SRC}
        ${DIC_MODULES_TIMELINE_CORE_DATABASE_SRC}
        ${DIC_MODULES_TIMELINE_CORE_DATABASE_HELPER_SRC}
        ${DIC_MODULES_TIMELINE_CORE_ANALYZE_SRC}
        ${DIC_MODULES_TIMELINE_CORE_CACHE_SRC}
        ${DIC_MODULES_TIMELINE_CORE_PARSER_SRC}
        ${DIC_MODULES_TIMELINE_CORE_TABLE_SRC}
        ${DIC_MODULES_TIMELINE_CORE_DATAENGINE_SRC}
        ${DIC_MODULES_TIMELINE_CORE_KERNELE2E_SRC}
        ${DIC_MODULES_TIMELINE_CORE_SYSTEMVIEW_SRC}
        ${DIC_MODULES_TIMELINE_HANDLER_SRC}
        ${DIC_MODULES_TIMELINE_PROTOCOL_SRC}
        ${DIC_MODULES_TIMELINE_PARSER_SRC}
        ${DIC_MODULES_TIMELINE_CACHE_SRC}
        ${DIC_MODULES_SUMMARY_SRC}
        ${DIC_MODULES_SUMMARY_CORE_SRC}
        ${DIC_MODULES_SUMMARY_HANDLER_SRC}
        ${DIC_MODULES_SUMMARY_PROTOCOL_SRC}
        ${DIC_MODULES_SUMMARY_DATABASE_SRC}
        ${DIC_MODULES_SUMMARY_PARSER_SRC}
        ${DIC_MODULES_TIMELINE_PROTOCOL_SRC}
        ${DIC_MODULES_MEMORY_SRC}
        ${DIC_MODULES_MEMORY_DATABASE_SRC}
        ${DIC_MODULES_MEMORY_PARSER_SRC}
        ${DIC_MODULES_MEMORY_HANDLER_SRC}
        ${DIC_MODULES_MEMORY_PROTOCOL_SRC}
        ${DIC_MODULES_MEMORY_TABLE_SRC}
        ${DIC_MODULES_MEMORY_SERVICE_SRC}
        ${DIC_MODULES_MEMORY_VIEW_SRC}
        ${DIC_MODULES_SUMMARY_PROTOCOL_SRC}
        ${DIC_MODULES_COMMUNICATION_SRC}
        ${DIC_MODULES_COMMUNICATION_DATABASE_SRC}
        ${DIC_MODULES_COMMUNICATION_HANDLER_SRC}
        ${DIC_MODULES_COMMUNICATION_PROTOCOL_SRC}
        ${DIC_MODULES_COMMUNICATION_PARSER_SRC}
        ${DIC_MODULES_COMMUNICATION_CORE_SRC}
        ${DIC_MODULES_COMMUNICATION_ADVISOR_SRC}
        ${DIC_MODULES_OPERATOR_SRC}
        ${DIC_MODULES_OPERATOR_DATABASE_SRC}
        ${DIC_MODULES_OPERATOR_PARSER_SRC}
        ${DIC_MODULES_OPERATOR_HANDLER_SRC}
        ${DIC_MODULES_OPERATOR_PROTOCOL_SRC}
        ${DIC_MODULES_FULL_DB_SRC}
        ${DIC_MODULES_FULL_DB_PARSER_SRC}
        ${DIC_MODULES_FULL_DB_PARSER_UNIT_SRC}
        ${DIC_MODULES_FULL_DB_DATABASE_SRC}
        ${DIC_MODULES_SOURCE_SRC}
        ${DIC_MODULES_SOURCE_PARSER_SRC}
        ${DIC_MODULES_SOURCE_HANDLER_SRC}
        ${DIC_MODULES_SOURCE_PROTOCOL_SRC}
        ${DIC_MODULES_SOURCE_CORE_SRC}
        ${DIC_MODULES_ADVISOR_SRC}
        ${DIC_MODULES_ADVISOR_HANDLER_SRC}
        ${DIC_MODULES_ADVISOR_PROTOCOL_SRC}
        ${DIC_MODULES_ADVISOR_PROCESS_SRC}
        ${DIC_MODULES_IE_SRC}
        ${DIC_MODULES_IE_API_SRC}
        ${DIC_MODULES_IE_CONTEXT_SRC}
        ${DIC_MODULES_IE_HANDLER_SRC}
        ${DIC_MODULES_IE_PROTOCOL_SRC}
        ${DIC_MODULES_IE_REPO_SRC}
        ${DIC_MODULES_MEM_SCOPE_SRC}
        ${DIC_MODULES_MEM_SCOPE_SVC_SRC}
        ${DIC_MODULES_MEM_SCOPE_DB_SRC}
        ${DIC_MODULES_MEM_SCOPE_HANDLER_SRC}
        ${DIC_MODULES_MEM_SCOPE_PROTOCOL_SRC}
        ${DIC_MODULES_MEM_SCOPE_ENTITY_SRC}
        ${DIC_MODULES_MEM_SCOPE_PARSER_SRC}
        ${DIC_MODULES_MEM_SNAPSHOT_SRC}
        ${DIC_MODULES_MEM_SNAPSHOT_DB_SRC}
        ${DIC_MODULES_MEM_SNAPSHOT_HANDLER_SRC}
        ${DIC_MODULES_MEM_SNAPSHOT_PROTOCOL_SRC}
        ${DIC_MODULES_MEM_SNAPSHOT_PARSER_SRC}
        ${DIC_MODULES_MEM_SNAPSHOT_SVC_SRC}
        ${DIC_MODULES_IE_REPO_SRC}
        ${DIC_MODULES_RL_SRC}
        ${DIC_MODULES_RL_CORE_SRC}
        ${DIC_MODULES_RL_PROTOCOL_SRC}
        ${DIC_MODULES_RL_HANDLER_SRC}
        ${DIC_MODULES_TRITON_SRC})
add_subdirectory(entry)
if (${_PROJECT_TYPE} MATCHES "test|fuzz")
    enable_testing()
    set(CMAKE_C_FLAGS_PROFILE --coverage)
    set(CMAKE_CXX_FLAGS_PROFILE --coverage)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    add_subdirectory(${HOME_DIR}/third_party/googletest googletest.out EXCLUDE_FROM_ALL)
    add_subdirectory(test)
elseif ($ENV{DEV_TYPE} MATCHES "true")
    enable_testing()
    add_subdirectory(${HOME_DIR}/third_party/googletest googletest.out EXCLUDE_FROM_ALL)
    add_subdirectory(test)
endif()
