#define _GNU_SOURCE

#include <stdio.h>

#include "../../include/raft.h"

#include "../../src/logger.h"

#include "../lib/munit.h"

/**
 *
 * Helpers
 *
 */

struct fixture
{
    struct raft_logger logger;
    struct raft_context ctx;
    struct
    {
        int level;
        char *message;
    } last; /* Last message emitted. */
};

static void fixture__emit(void *data,
                   struct raft_context *ctx,
                   int level,
                   const char *format,
                   ...)
{
    struct fixture *f = data;
    va_list args;
    int rv;

    munit_assert_ptr_equal(ctx, &f->ctx);

    f->last.level = level;

    va_start(args, format);
    rv = vasprintf(&f->last.message, format, args);
    va_end(args);

    munit_assert_int(rv, >=, strlen(format));
}

/**
 *
 * Setup and tear down
 *
 */

static void *setup(const MunitParameter params[], void *user_data)
{
    struct fixture *f = munit_malloc(sizeof *f);

    (void)user_data;
    (void)params;

    f->logger.data = f;
    f->logger.emit = fixture__emit;

    return f;
}

static void tear_down(void *data)
{
    struct fixture *f = data;

    free(f);
}

/**
 *
 * raft__debugf
 *
 */

/* Emit a message at debug level. */
static MunitResult test_debugf(const MunitParameter params[], void *data)
{
    struct fixture *f = data;

    (void)params;

    raft__debugf(f, "hello");

    munit_assert_int(f->last.level, ==, RAFT_DEBUG);
    munit_assert_string_equal(f->last.message, "hello");

    free(f->last.message);

    /* Use the default logger */
    f->logger = raft_default_logger;
    raft__debugf(f, "hello");

    return MUNIT_OK;
}

/* Emit a message at info level, with arguments. */
static MunitResult test_infof(const MunitParameter params[], void *data)
{
    struct fixture *f = data;

    (void)params;

    raft__infof(f, "hello %s", "world");

    munit_assert_int(f->last.level, ==, RAFT_INFO);
    munit_assert_string_equal(f->last.message, "hello world");

    free(f->last.message);

    /* Use the default logger */
    f->logger = raft_default_logger;
    raft__infof(f, "hello %s", "world");

    return MUNIT_OK;
}

/* Emit a message at warn level, with arguments. */
static MunitResult test_warnf(const MunitParameter params[], void *data)
{
    struct fixture *f = data;

    (void)params;

    raft__warnf(f, "hello %d", 123);

    munit_assert_int(f->last.level, ==, RAFT_WARN);
    munit_assert_string_equal(f->last.message, "hello 123");

    free(f->last.message);

    /* Use the default logger */
    f->logger = raft_default_logger;
    raft__warnf(f, "hello %d", 123);

    return MUNIT_OK;
}

/* Emit a message at error level, with arguments. */
static MunitResult test_errorf(const MunitParameter params[], void *data)
{
    struct fixture *f = data;

    (void)params;

    raft__errorf(f, "hello %d %s", 123, "world");

    munit_assert_int(f->last.level, ==, RAFT_ERROR);
    munit_assert_string_equal(f->last.message, "hello 123 world");

    free(f->last.message);

    /* Use the default logger */
    f->logger = raft_default_logger;
    raft__errorf(f, "hello %d %s", 123, "world");

    return MUNIT_OK;
}

/* Emit a message at unknown level. */
static MunitResult test_unknown_level(const MunitParameter params[], void *data)
{
    struct fixture *f = data;

    (void)params;

    raft_default_logger.emit(NULL, &f->ctx, 666, "hello");

    return MUNIT_OK;
}

static MunitTest macros_tests[] = {
    {"/debugf", test_debugf, setup, tear_down, 0, NULL},
    {"/infof", test_infof, setup, tear_down, 0, NULL},
    {"/warnf", test_warnf, setup, tear_down, 0, NULL},
    {"/errorf", test_errorf, setup, tear_down, 0, NULL},
    {"/unknown-level", test_unknown_level, setup, tear_down, 0, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

/**
 *
 * Test suite
 *
 */

MunitSuite raft_logger_suites[] = {
    {"", macros_tests, NULL, 1, 0},
    {NULL, NULL, NULL, 0, 0},
};
