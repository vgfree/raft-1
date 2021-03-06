#include "../lib/munit.h"

extern MunitSuite raft_client_suites[];
extern MunitSuite raft_configuration_suites[];
extern MunitSuite raft_context_suites[];
extern MunitSuite raft_election_suites[];
extern MunitSuite raft_encoding_suites[];
extern MunitSuite raft_io_suites[];
extern MunitSuite raft_log_suites[];
extern MunitSuite raft_logger_suites[];
extern MunitSuite raft_replication_suites[];
extern MunitSuite raft_rpc_suites[];
extern MunitSuite raft_tick_suites[];
extern MunitSuite raft_suites[];

static MunitSuite suites[] = {
    {"client", NULL, raft_client_suites, 1, 0},
    {"configuration", NULL, raft_configuration_suites, 1, 0},
    {"context", NULL, raft_context_suites, 1, 0},
    {"election", NULL, raft_election_suites, 1, 0},
    {"encoding", NULL, raft_encoding_suites, 1, 0},
    {"io", NULL, raft_io_suites, 1, 0},
    {"log", NULL, raft_log_suites, 1, 0},
    {"logger", NULL, raft_logger_suites, 1, 0},
    {"replication", NULL, raft_replication_suites, 1, 0},
    {"rpc", NULL, raft_rpc_suites, 1, 0},
    {"tick", NULL, raft_tick_suites, 1, 0},
    {"raft", NULL, raft_suites, 1, 0},
    {NULL, NULL, NULL, 0, 0},
};

static MunitSuite suite = {(char *)"", NULL, suites, 1, 0};

/* Test runner executable */
int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)])
{
    return munit_suite_main(&suite, (void *)"unit", argc, argv);
}
