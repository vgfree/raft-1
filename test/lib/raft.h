/**
 * Helpers to initialize a raft object, as if its state was loaded from disk.
 */

#ifndef TEST_RAFT_H
#define TEST_RAFT_H

#include "../../include/raft.h"

/**
 * Load the persistent state of the given raft instance (term, vote and log)
 * from its test I/O implementation.
 */
void test_load(struct raft *r);

/**
 * Bootstrap a raft instance.
 *
 * The initial configuration will have the given amount of servers and will be
 * saved as first entry in the log. The server IDs are assigned sequentially
 * starting from 1 up to @n_servers. Only servers with IDs in the range
 * [@voting_a, @voting_b] will be voting servers.
 */
void test_bootstrap_and_load(struct raft *r,
                             int n_servers,
                             int voting_a,
                             int voting_b);

/**
 * Make a pristine raft instance transition to the leader state, by getting
 * votes from a majority of the servers in the configuration.
 */
void test_become_leader(struct raft *r);

/**
 * Receive a valid heartbeat request from the given leader. Valid means that the
 * term of the request will match @r's current term, and the previous index/term
 * will match @r's last log entry.
 */
void test_receive_heartbeat(struct raft *r, uint64_t leader_id);

#endif /* TEST_CONFIGURATION_H */
