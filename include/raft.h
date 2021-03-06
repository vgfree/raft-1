#ifndef RAFT_H_
#define RAFT_H_

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Error codes.
 */
enum {
    RAFT_ERR_NOMEM = 1,
    RAFT_ERR_INTERNAL,
    RAFT_ERR_BAD_SERVER_ID,
    RAFT_ERR_DUP_SERVER_ID,
    RAFT_ERR_NO_SERVER_ADDRESS,
    RAFT_ERR_EMPTY_CONFIGURATION,
    RAFT_ERR_CONFIGURATION_NOT_EMPTY,
    RAFT_ERR_MALFORMED,
    RAFT_ERR_NO_SPACE,
    RAFT_ERR_BUSY,
    RAFT_ERR_IO_BUSY,
    RAFT_ERR_NOT_LEADER,
    RAFT_ERR_SHUTDOWN,
};

/**
 * Map error codes to error messages.
 */
#define RAFT_ERRNO_MAP(X)                                                 \
    X(RAFT_ERR_NOMEM, "out of memory")                                    \
    X(RAFT_ERR_BAD_SERVER_ID, "server ID is not valid")                   \
    X(RAFT_ERR_DUP_SERVER_ID, "a server with the same ID already exists") \
    X(RAFT_ERR_NO_SERVER_ADDRESS, "server has no address")                \
    X(RAFT_ERR_EMPTY_CONFIGURATION, "configuration has no servers")       \
    X(RAFT_ERR_CONFIGURATION_NOT_EMPTY, "configuration has servers")      \
    X(RAFT_ERR_MALFORMED, "encoded data is malformed")                    \
    X(RAFT_ERR_NO_SPACE, "no space left on device")                       \
    X(RAFT_ERR_BUSY, "an append entries request is already in progress")  \
    X(RAFT_ERR_IO_BUSY, "a log write request is already in progress")

/**
 * Return the error message describing the given error code.
 */
const char *raft_strerror(int errnum);

/**
 * User-definable dynamic memory allocation routines.
 */
struct raft_heap
{
    void *data; /* User data */
    void *(*malloc)(void *data, size_t size);
    void (*free)(void *data, void *ptr);
    void *(*calloc)(void *data, size_t nmemb, size_t size);
    void *(*realloc)(void *data, void *ptr, size_t size);
};

void *raft_malloc(size_t size);
void raft_free(void *ptr);
void *raft_calloc(size_t nmemb, size_t size);
void *raft_realloc(void *ptr, size_t size);

/**
 * Use a custom dynamic memory allocator.
 */
void raft_heap_set(struct raft_heap *heap);

/**
 * Use the default dynamic memory allocator (from the stdlib). This clears any
 * custom allocator specified with @raft_heap_set.
 */
void raft_heap_set_default();

/**
 * Size of the errmsg buffer of raft_context, holding a human-readable text
 * describing the last error occurred.
 */
#define RAFT_CONTEXT_ERRMSG_SIZE 1024

/**
 * Hold the value of a raft term. Guaranteed to be at least 64-bit long.
 */
typedef unsigned long long raft_term;

/**
 * Hold the value of a raft entry index. Guaranteed to be at least 64-bit long.
 */
typedef unsigned long long raft_index;

/**
 * Hold contextual information about current raft's state. This information is
 * meant to be included in log and error messages.
 *
 * Pointers will be set to NULL when the information is not available.
 */
struct raft_context
{
    unsigned short *state;
    raft_term *current_term;
    char errmsg[RAFT_CONTEXT_ERRMSG_SIZE];
};

/**
 * Format context information as a simple string, e.g.:
 *
 *   "(state=follower term=2)"
 *
 * Null fields will be omitted.
 */
void raft_context_format(char *str, size_t size, struct raft_context *ctx);

/**
 * Logging levels.
 */
enum { RAFT_DEBUG, RAFT_INFO, RAFT_WARN, RAFT_ERROR };

/**
 * Handle log messages at different levels.
 */
struct raft_logger
{
    void *data;
    void (*emit)(void *, struct raft_context *, int, const char *, ...);
};

/**
 * Default logger, emitting messages to stderr.
 */
extern struct raft_logger raft_default_logger;

/**
 * A data buffer.
 */
struct raft_buffer
{
    void *base; /* Pointer to the buffer data */
    size_t len; /* Length of the buffer. */
};

/**
 * Hold information about a single server in the cluster configuration.
 */
struct raft_server
{
    unsigned id;         /* Server ID, must be greater than zero. */
    const char *address; /* Server address. User defined. */
    bool voting;         /* Whether this is a voting server. */
};

/**
 * Hold information about all servers part of the cluster.
 */
struct raft_configuration
{
    struct raft_server *servers; /* Array of servers member of the cluster. */
    unsigned n;                  /* Number of servers in the array. */
};

void raft_configuration_init(struct raft_configuration *c);

void raft_configuration_close(struct raft_configuration *c);

/**
 * Add a server to a raft configuration. The given ID must not be already in use
 * by another server in the configuration.
 */
int raft_configuration_add(struct raft_configuration *c,
                           const unsigned id,
                           const char *address,
                           const bool voting);

/**
 * Log entry types.
 */
enum { RAFT_LOG_COMMAND, RAFT_LOG_CONFIGURATION };

/**
 * A single entry in the raft log.
 *
 * From Figure 3.1:
 *
 *   Each contains [either a] command for the state machine [or a configuration
 *   change], and term when entry was received by the leader.
 *
 * An entry that originated from this raft instance while it was the leader
 * (typically via client calls to raft_accept()) should normaly have a @buf
 * attribute that points directly to the memory that was originally allocated to
 * contain then entry data, and the @batch attribute is set to #NULL.
 *
 * An entry that was received from the network upon an AppendEntries RPC or that
 * was loaded from disk at startup should normally have a @batch attribute that
 * points to a contiguous chunk of memory containing the data of the entry
 * itself plus possibly the data for other entries that were received or loaded
 * with it in the same request. In this case the @buf pointer will be equal to
 * the @batch pointer plus an offset, that locates the position of the entry's
 * data within the batch.
 *
 * When the @batch attribute is not #NULL the raft library will take care of
 * releasing that memory only once there are no more references to the
 * associated entries.
 *
 * This arrangement makes it possible to perform "zero copy" I/O in most cases.
 */
struct raft_entry
{
    raft_term term;         /* Term in which the entry was created */
    unsigned short type;    /* Entry type (FSM command or config change) */
    struct raft_buffer buf; /* Entry data */
    void *batch;            /* Batch that buf's memory points to, if any. */
};

/**
 * Counter for outstanding references to a log entry. When an entry is first
 * appended to the log, its refcount is set to one (the log itself is the only
 * one referencing the entry). Whenever an entry is included in an I/O request
 * (write entries to disk or send entries to other servers) its refcount is
 * increased by one. Whenever an entry gets deleted from the log its refcount is
 * decreased by one, likewise whenever an I/O request is completed the refcount
 * of the relevant entries is decreased by one. When the refcount drops to zero
 * the memory pointed to by its @buf attribute gets released, or if the @batch
 * attribute is non-NULL a check is made to see if there's any other entry of
 * the same batch with a non-zero refcount, and the memory pointed at by @batch
 * itself is releaed if there's no such other entry.
 */
struct raft_entry_ref
{
    raft_term term;              /* Term of the entry being ref-counted */
    raft_index index;            /* Index of the entry being ref-counted */
    unsigned short count;        /* Number of references */
    struct raft_entry_ref *next; /* Next item in the bucket (for collisions) */
};

/**
 * In-memory cache of the persistent raft log stored on disk.
 *
 * The raft log cache is implemented as a circular buffer of log entries, which
 * makes some common operations (e.g. deleting the first N entries when
 * snapshotting) very efficient.
 */
struct raft_log
{
    struct raft_entry *entries;  /* Buffer of log entries. */
    size_t size;                 /* Number of available slots in the buffer */
    size_t front, back;          /* Indexes of used slots [front, back). */
    raft_index offset;               /* Index offest of the first entry. */
    struct raft_entry_ref *refs; /* Log entries reference counts hash table */
    size_t refs_size;            /* Size of the reference counts hash table */
};

/**
 * Hold the arguments of a RequestVote RPC (figure 3.1).
 *
 * The RequestVote RPC is invoked by candidates to gather votes (figure 3.1).
 */
struct raft_request_vote_args
{
    raft_term term;            /* Candidate's term. */
    unsigned candidate_id;     /* ID of the server requesting the vote. */
    raft_index last_log_index; /* Index of candidate's last log entry. */
    raft_index last_log_term;  /* Term of log entry at last_log_index. */
};

/**
 * Hold the result of a RequestVote RPC (figure 3.1).
 */
struct raft_request_vote_result
{
    raft_term term;    /* Receiver's current_term (candidate updates itself). */
    bool vote_granted; /* True means candidate received vote. */
};

/**
 * Hold the arguments of an AppendEntries RPC.
 *
 * The AppendEntries RPC is invoked by the leader to replicate log entries. It's
 * also used as heartbeat (figure 3.1).
 */
struct raft_append_entries_args
{
    raft_term term;             /* Leader's term. */
    unsigned leader_id;         /* So follower can redirect clients. */
    raft_index prev_log_index;  /* Index of log entry preceeding new ones. */
    raft_term prev_log_term;    /* Term of entry at prev_log_index. */
    raft_index leader_commit;   /* Leader's commit_index. */
    struct raft_entry *entries; /* Log entries to append. */
    unsigned n;                 /* Size of the log entries array. */
};

/**
 * Hold the result of an AppendEntries RPC (figure 3.1).
 */
struct raft_append_entries_result
{
    raft_term term; /* Receiver's current_term, for leader to update itself. */
    bool success; /* True if follower had entry matching prev_log_index/term. */
    raft_index last_log_index; /* Receiver's last log entry index, as hint */
};

/**
 * Interface providing raft-related disk and network I/O primitives.
 */
struct raft_io
{
    int version; /* API version implemented by this instance. Currently %1. */
    void *data;  /* Custom user data. */

    /**
     * Synchronously persist current term (and nil vote). The implementation
     * MUST ensure that the change is durable before returning (e.g. using
     * fdatasync() or #O_DIRECT).
     */
    int (*write_term)(struct raft_io *io, const raft_term term);

    /**
     * Synchronously persist who we voted for. The implementation MUST ensure
     * that the change is durable before returning (e.g. using fdatasync() or
     * #O_DIRECT).
     */
    int (*write_vote)(struct raft_io *io, const unsigned server_id);

    /**
     * Asynchronously append the given entries to the log.
     *
     * At most one write log request can be in flight at any given time. The
     * implementation must return @RAFT_ERR_IO_BUSY if a new request is
     * submitted before the previous one is completed.
     *
     * The implementation is guaranteed that the memory holding the given
     * entries will not be released until a notification is fired by invoking
     * the raft_handle_io() callback with the given request ID.
     */
    int (*write_log)(struct raft_io *io,
                     const unsigned request_id,
                     const struct raft_entry entries[],
                     const unsigned n);

    /**
     * Synchronously delete all log entries from the given index onwards.
     */
    int (*truncate_log)(struct raft_io *io, const raft_index index);

    /**
     * Asynchronously invoke a RequestVote RPC on the given @server. The
     * implementation can ignore transport errors happening after this function
     * has returned.
     */
    int (*send_request_vote_request)(struct raft_io *io,
                                     const struct raft_server *server,
                                     const struct raft_request_vote_args *args);
    /**
     * Asynchronously reply to a RequestVote RPC from the given @server. The
     * implementation can ignore transport errors happening after this function
     * has returned.
     */
    int (*send_request_vote_response)(struct raft_io *io,
                                      const struct raft_server *server,
                                      const struct raft_request_vote_result *);
    /**
     * Asynchronously invoke an AppendEntries RPC on the given @server.
     *
     * The implementation is guaranteed that the memory holding the given
     * entries will not be released until raft_handle_io_comleted() is called in
     * order to notify the raft library that the send request has completed.
     *
     * The implementation can ignore transport errors happening after this
     * function has returned, but it still must notify the raft library that the
     * request has been completed unsuccessfully.
     */
    int (*send_append_entries_request)(struct raft_io *io,
                                       const unsigned request_id,
                                       const struct raft_server *server,
                                       const struct raft_append_entries_args *);

    /**
     * Asynchronously reply to an AppendEntries RPC from the given @server. The
     * implementation can ignore transport errors happening after this function
     * has returned.
     */
    int (*send_append_entries_response)(
        struct raft_io *io,
        const struct raft_server *server,
        const struct raft_append_entries_result *);
};

/**
 * Type codes for raft I/O requests.
 */
enum {
    RAFT_IO_NULL = 0,
    RAFT_IO_WRITE_LOG,
    RAFT_IO_APPEND_ENTRIES,
    RAFT_IO_APPEND_ENTRIES_RESULT,
    RAFT_IO_REQUEST_VOTE,
    RAFT_IO_REQUEST_VOTE_RESULT
};

/**
 * Hold information about an in-flight I/O request submitted to a @raft_io
 * instance that references either log entries or snapshots.
 */
struct raft_io_request
{
    unsigned short type;        /* Type of the pending I/O request. */
    raft_index index;           /* Index of the first entry in the request. */
    struct raft_entry *entries; /* Entries referenced in the request. */
    unsigned n;                 /* Length of the entries array. */
    unsigned leader_id;         /* Leader that generated this entry. */
    raft_index leader_commit;   /* Last known leader commit index. */
};

/**
 * Server state codes.
 */
enum { RAFT_STATE_FOLLOWER, RAFT_STATE_CANDIDATE, RAFT_STATE_LEADER };

/**
 * Server state names ('follower', 'candidate', 'leader'), indexed by state
 * code.
 */
extern const char *raft_state_names[];

/**
 * Event types IDs.
 */
enum {
    /**
     * Fired when the server state changes. The initial state is always
     */
    RAFT_EVENT_STATE_CHANGE = 0
};

/**
 * Number of available event types.
 */
#define RAFT_EVENT_N (RAFT_EVENT_STATE_CHANGE + 1)

/**
 * Hold and drive the state of a single raft server in a cluster.
 */
struct raft
{
    /**
     * User-defined disk and network I/O interface implementation.
     */
    struct raft_io *io;

    /**
     * Server ID of this raft instance.
     */
    unsigned id;

    /**
     * Custom user data. It will be passed back to callbacks registered with
     * raft_watch().
     */
    void *data;

    /**
     * The fields below are a cache of the server's persistent state, updated on
     * stable storage before responding to RPCs (Figure 3.1).
     */
    raft_term current_term; /* Latest term server has seen. */
    unsigned voted_for;     /* Candidate that received vote in current term. */
    struct raft_log log;    /* Log entries. */

    /**
     * Membership configuration (Chapter 4).
     */
    struct raft_configuration configuration;

    /**
     * Election timeout in milliseconds (default 1000).
     *
     * From 3.4:
     *
     *   Raft uses a heartbeat mechanism to trigger leader election. When
     *   servers start up, they begin as followers. A server remains in follower
     *   state as long as it receives valid RPCs from a leader or
     *   candidate. Leaders send periodic heartbeats (AppendEntries RPCs that
     *   carry no log entries) to all followers in order to maintain their
     *   authority. If a follower receives no communication over a period of
     *   time called the election timeout, then it assumes there is no viable
     *   leader and begins an election to choose a new leader.
     *
     * This is the baseline value and will be randomized between 1x and 2x.
     *
     * See raft_change_election_timeout() to customize the value of this
     * attribute.
     */
    unsigned election_timeout;

    /**
     * Heartbeat timeout in milliseconds (default 500). This is relevant only
     * for when the raft instance is in leader state: empty AppendEntries RPCs
     * will be sent if this amount of milliseconds elapses without any
     * user-triggered AppendEntries RCPs being sent.
     *
     * From Figure 3.1:
     *
     *   [Leaders] Send empty AppendEntries RPC during idle periods to prevent
     *   election timeouts.
     */
    unsigned heartbeat_timeout;

    /**
     * Logger to use to emit messages (default stdout);
     */
    struct raft_logger logger;

    /**
     * The fields below hold the part of the server's volatile state which
     * is always applicable regardless of the whether the server is
     * follower, candidate or leader (Figure 3.1). This state is rebuilt
     * automatically after a server restart.
     */
    raft_index commit_index; /* Highest log entry known to be committed */
    raft_index last_applied; /* Highest log entry applied to the FSM */

    /**
     * Current server state of this raft instance, along with a union defining
     * state-specific values.
     */
    unsigned short state;
    union {
        struct
        {
            /**
             * The fields below hold the part of the server's volatile state
             * which is specific to followers.
             */
            const struct raft_server *current_leader;
        } follower_state;

        struct
        {
            /**
             * The fields below hold the part of the server's volatile state
             * which is specific to leaders (Figure 3.1). This state is
             * reinitialized after the server gets elected.
             */
            raft_index *next_index;  /* For each server, next entry to send */
            raft_index *match_index; /* For each server, highest applied idx */
        } leader_state;

        struct
        {
            /**
             * The fields below hold the part of the server's volatile state
             * which is specific to candidates. This state is reinitialized
             * after the server starts a new election round.
             */
            bool *votes; /* For each server, whether vote was granted */
        } candidate_state;
    };

    /**
     * Random generator. Defaults to stdlib rand().
     */
    int (*rand)();
    /**

     * Current election timeout. Randomized from election_timeout.
     *
     * From §9.3:
     *
     *   We recommend using a timeout range that is ten times the one-way
     *   network latency (even if the true network latency is five times greater
     *   than anticipated, most clusters would still be able to elect a leader
     *   in a timely manner).
     */
    unsigned election_timeout_rand;

    /**
     * For followers and candidates, time elapsed since the last election
     * started, in millisecond. For leaders time elapsed since the last
     * AppendEntries RPC, in milliseconds.
     */
    unsigned timer;

    /**
     * Registered watchers.
     */
    void (*watchers[RAFT_EVENT_N])(void *, int);

    /**
     * Log and error context.
     */
    struct raft_context ctx;

    /**
     * Hold information about in-flight I/O requests that involve memory shared
     * between this raft instance an its I/O implementation.
     */
    struct
    {
        struct raft_io_request *requests;
        unsigned size;
    } io_queue;
};

/**
 * Initialize a raft server object.
 */
void raft_init(struct raft *r,
               struct raft_io *io,
               void *data,
               const unsigned id);

/**
 * Close a raft instance, deallocating all used resources.
 */
void raft_close(struct raft *r);

/**
 * Set a custom logger.
 */
void raft_set_logger(struct raft *r, const struct raft_logger *logger);

/**
 * Set a custom rand() function.
 */
void raft_set_rand(struct raft *r, int (*rand)());

/**
 * Set the election timeout.
 *
 * Every raft instance is initialized with a default election timeout of 1000
 * milliseconds. If you wish to tweak it, call this function before starting
 * your event loop.
 *
 * From Chapter 9:
 *
 *   We recommend a range that is 10–20 times the one-way network latency, which
 *   keeps split votes rates under 40% in all cases for reasonably sized
 *   clusters, and typically results in much lower rates.
 */
void raft_set_election_timeout_(struct raft *r,
                                const unsigned election_timeout);

/**
 * Human readable version of the current state.
 */
const char *raft_state_name(struct raft *r);

/**
 * Notify the raft instance that a certain amout of time as elapsed.
 *
 * User code needs to call this function periodically, in order to
 * process events that are dependent on time passing.
 */
int raft_tick(struct raft *r, const unsigned msec_since_last_tick);

/**
 * Accept a client request to append new FSM commands to the log.
 *
 * If this server is the leader, it will create @n new log entries of type
 * #RAFT_LOG_COMMAND using the given buffers as their payloads, append them to
 * its own log and attempt to replicate them on other servers by sending
 * AppendEntries RPCs.
 *
 * The memory pointed at by the @base attribute of each #raft_buffer in the
 * given array must have been allocated with raft_malloc() or a compatible
 * allocator. If this function returns 0, the ownership of this memory is
 * implicitely transferred to the raft library, which will take care of
 * releasing it when appropriate. Any further client access to such memory leads
 * to undefined behavior.
 */
int raft_accept(struct raft *r,
                const struct raft_buffer bufs[],
                const unsigned n);

/**
 * Register a callback to be fired upon the given event.
 *
 * The @cb callback will be invoked the next time the event with the given ID
 * occurs and will be passed back the @data pointer set on @r and the event ID.
 *
 * At most one callback can be registered for each event. Passing a NULL
 * callback disable notifications for that event.
 */
void raft_watch(struct raft *r, int event, void (*cb)(void *, int));

/**
 * Process the result of an asynchronous I/O request that involves raft entries
 * or snapshots (i.e. memory shared between a raft instance and its I/O
 * implementation).
 *
 * The @request parameter holds information about the entries or snapshosts
 * referenced in request that has been completed. The @status parameter must be
 * set to zero if the write was successful, or non-zero otherwise.
 */
void raft_handle_io(struct raft *r,
                    const unsigned request_id,
                    const int status);

/**
 * Process a RequestVote RPC from the given server.
 *
 * This function must be invoked whenever the user's transport implementation
 * receives a RequestVote RPC request from another server.
 */
int raft_handle_request_vote(struct raft *r,
                             const struct raft_server *server,
                             const struct raft_request_vote_args *args);

/**
 * Process an AppendEntries RPC result from the given server.
 *
 * This function must be invoked whenever the user's transport implementation
 * receives an AppendEntries RPC result from another server.
 */
int raft_handle_request_vote_response(
    struct raft *r,
    const struct raft_server *server,
    const struct raft_request_vote_result *result);

/**
 * Process an AppendEntries RPC from the given server.
 *
 * This function must be invoked whenever the user's transport implementation
 * receives an AppendEntries RPC request from another server.
 */
int raft_handle_append_entries(struct raft *r,
                               const struct raft_server *server,
                               const struct raft_append_entries_args *args);

/**
 * Process an AppendEntries RPC result from the given server.
 *
 * This function must be invoked whenever the user's transport implementation
 * receives an AppendEntries RPC result from another server.
 */
int raft_handle_append_entries_response(
    struct raft *r,
    const struct raft_server *server,
    const struct raft_append_entries_result *result);

/**
 * Encode a raft configuration object. The memory of the returned buffer is
 * allocated using raft_malloc(), and client code is responsible for releasing
 * it when no longer needed. The raft library makes no use of that memory after
 * this function returns.
 */
int raft_encode_configuration(const struct raft_configuration *c,
                              struct raft_buffer *buf);

/**
 * Populate a configuration object by decoding the given serialized payload.
 */
int raft_decode_configuration(const struct raft_buffer *buf,
                              struct raft_configuration *c);

int raft_encode_append_entries(const struct raft_append_entries_args *args,
                               struct raft_buffer *buf);

int raft_decode_append_entries(const struct raft_buffer *buf,
                               struct raft_append_entries_args *args);

/**
 * The layout of the memory pointed at by a @batch pointer is the following:
 *
 * [8 bytes] Number of entries in the batch, little endian.
 * [header1] Header data of the first entry of the batch.
 * [  ...  ] More headers
 * [headerN] Header data of the last entry of the batch.
 * [data1  ] Payload data of the first entry of the batch.
 * [  ...  ] More data
 * [dataN  ] Payload data of the last entry of the batch.
 *
 * An entry header is 16-byte long and has the following layout:
 *
 * [8 bytes] Term in which the entry was created, little endian.
 * [1 byte ] Message type (Either RAFT_LOG_COMMAND or RAFT_LOG_CONFIGURATION)
 * [3 bytes] Currently unused.
 * [4 bytes] Size of the log entry data, little endian.
 *
 * A payload data section for an entry is simply a sequence of bytes of
 * arbitrary lengths, possibly padded with extra bytes to reach 8-byte boundary
 * (which means that all entry data pointers are 8-byte aligned).
 */
int raft_decode_entries_batch(const struct raft_buffer *buf,
                              struct raft_entry *entries,
                              unsigned n);

int raft_encode_append_entries_result(
    const struct raft_append_entries_result *result,
    struct raft_buffer *buf);

int raft_decode_append_entries_result(
    const struct raft_buffer *buf,
    struct raft_append_entries_result *result);

int raft_encode_request_vote(const struct raft_request_vote_args *args,
                             struct raft_buffer *buf);

int raft_decode_request_vote(const struct raft_buffer *buf,
                             struct raft_request_vote_args *args);

int raft_encode_request_vote_result(
    const struct raft_request_vote_result *result,
    struct raft_buffer *buf);

int raft_decode_request_vote_result(const struct raft_buffer *buf,
                                    struct raft_request_vote_result *result);

#endif /* RAFT_H_ */
