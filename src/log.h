/**
 *
 * In-memory cache of the persistent raft log stored on disk.
 *
 */

#ifndef RAFT_LOG_H
#define RAFT_LOG_H

#include "../include/raft.h"

void raft_log__init(struct raft_log *l);

void raft_log__close(struct raft_log *l);

/**
 * Append the an entry to the log.
 */
int raft_log__append(struct raft_log *l,
                     const raft_term term,
                     const int type,
                     const struct raft_buffer *buf,
                     void *batch);

/**
 * Get the current number of entries in the log.
 */
size_t raft_log__n_entries(struct raft_log *l);

/**
 * Get the index of the first entry in the log.
 */
raft_index raft_log__first_index(struct raft_log *l);

/**
 * Get the index of the last entry in the log.
 */
raft_index raft_log__last_index(struct raft_log *l);

/**
 * Get the term of the entry with the given index.
 */
raft_term raft_log__term_of(struct raft_log *l, const uint64_t index);

/**
 * Get the term of the last entry in the log.
 */
raft_term raft_log__last_term(struct raft_log *l);

/**
 * Get the entry with the given index.
 */
const struct raft_entry *raft_log__get(struct raft_log *l,
                                       const raft_index index);

/**
 * Acquire an array of entries from the given index onwards.
 *
 * The the payload memory referenced by *buf attribute of the returned entries
 * is guaranteed to be valid until raft_log__release() is called.
 */
int raft_log__acquire(struct raft_log *l,
                      const raft_index index,
                      struct raft_entry *entries[],
                      unsigned *n);

/**
 * Release a previously acquired array of entries.
 */
void raft_log__release(struct raft_log *l,
                       const raft_index index,
                       struct raft_entry entries[],
                       const size_t n);

/**
 * Delete all entries from the given index (included) onwards.
 */
void raft_log__truncate(struct raft_log *l, const raft_index index);

/**
 * Delete all entries up to the given index (included).
 */
void raft_log__shift(struct raft_log *l, const raft_index index);

#endif /* RAFT_LOG_H */
