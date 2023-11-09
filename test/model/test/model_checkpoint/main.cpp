/*-
 * Public Domain 2014-present MongoDB, Inc.
 * Public Domain 2008-2014 WiredTiger, Inc.
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#include "wiredtiger.h"
extern "C" {
#include "test_util.h"
}

#include "model/test/util.h"
#include "model/test/wiredtiger_util.h"
#include "model/kv_database.h"
#include "model/util.h"

/*
 * Command-line arguments.
 */
#define SHARED_PARSE_OPTIONS "h:p"

static char home[PATH_MAX]; /* Program working dir */
static TEST_OPTS *opts, _opts;

extern int __wt_optind;
extern char *__wt_optarg;

static void usage(void) WT_GCC_FUNC_DECL_ATTRIBUTE((noreturn));

/*
 * Configuration.
 */
#define ENV_CONFIG                                            \
    "cache_size=20M,create,"                                  \
    "debug_mode=(table_logging=true,checkpoint_retention=5)," \
    "eviction_updates_target=20,eviction_updates_trigger=90," \
    "log=(enabled,file_max=10M,remove=true),session_max=100," \
    "statistics=(all),statistics_log=(wait=1,json,on_close)"

/*
 * test_checkpoint --
 *     The basic test of the checkpoint model.
 */
static void
test_checkpoint(void)
{
    model::kv_database database;
    model::kv_table_ptr table = database.create_table("table");

    /* Keys. */
    const model::data_value key1("Key 1");
    const model::data_value key2("Key 2");
    const model::data_value key3("Key 3");
    const model::data_value key4("Key 4");
    const model::data_value key5("Key 5");

    /* Values. */
    const model::data_value value1("Value 1");
    const model::data_value value2("Value 2");
    const model::data_value value3("Value 3");
    const model::data_value value4("Value 4");
    const model::data_value value5("Value 5");

    /* Transactions. */
    model::kv_transaction_ptr txn1, txn2;

    /* Add some data. */
    txn1 = database.begin_transaction();
    testutil_check(table->insert(txn1, key1, value1));
    txn1->commit(10);
    txn1 = database.begin_transaction();
    testutil_check(table->insert(txn1, key2, value2));
    txn1->commit(20);

    /* Create a named and an unnamed checkpoint. */
    model::kv_checkpoint_ptr ckpt1 = database.create_checkpoint("ckpt1");

    /* Set the stable timestamp and create an unnamed checkpoint. */
    database.set_stable_timestamp(15);
    model::kv_checkpoint_ptr ckpt = database.create_checkpoint();

    /* Add more data. */
    txn1 = database.begin_transaction();
    testutil_check(table->insert(txn1, key3, value3));
    txn1->commit(30);

    /* Verify that we have the data that we expect. */
    testutil_assert(table->get(ckpt1, key1) == value1);
    testutil_assert(table->get(ckpt1, key2) == value2); /* The stable timestamp is not yet set. */
    testutil_assert(table->get(ckpt1, key3) == model::NONE);
    testutil_assert(table->get(ckpt, key1) == value1);
    testutil_assert(table->get(ckpt, key2) == model::NONE);
    testutil_assert(table->get(ckpt, key3) == model::NONE);

    /* Verify that we have the data that we expect - with read timestamps. */
    testutil_assert(table->get(ckpt1, key1, 15) == value1);
    testutil_assert(table->get(ckpt1, key2, 15) == model::NONE);
    testutil_assert(table->get(ckpt1, key3, 15) == model::NONE);

    /* Add two more keys; check that only that committed data are included. */
    txn1 = database.begin_transaction();
    txn2 = database.begin_transaction();
    testutil_check(table->insert(txn1, key4, value4));
    testutil_check(table->insert(txn2, key5, value5));
    txn1->commit(40);
    database.set_stable_timestamp(40);
    model::kv_checkpoint_ptr ckpt2 = database.create_checkpoint("ckpt2");
    testutil_assert(table->get(ckpt2, key3) == value3);
    testutil_assert(table->get(ckpt2, key4) == value4);
    testutil_assert(table->get(ckpt2, key5) == model::NONE);
    txn2->commit(50);

    /* Test with prepared transactions. */
    txn1 = database.begin_transaction();
    txn2 = database.begin_transaction();
    testutil_check(table->insert(txn1, key1, value4));
    testutil_check(table->insert(txn2, key2, value5));
    txn1->prepare(55);
    txn2->prepare(55);
    txn1->commit(60, 60);
    txn2->commit(60, 65);
    database.set_stable_timestamp(60);
    model::kv_checkpoint_ptr ckpt3 = database.create_checkpoint("ckpt3");
    testutil_assert(table->get(ckpt3, key1) == value4);
    testutil_assert(table->get(ckpt3, key2) == value2); /* The old value. */
    testutil_assert(table->get(ckpt3, key3) == value3);

    /* Test moving the stable timestamp backwards - this should fail silently. */
    database.set_stable_timestamp(50);
    testutil_assert(database.stable_timestamp() == 60);
    model::kv_checkpoint_ptr ckpt4 = database.create_checkpoint("ckpt4");
    testutil_assert(table->get(ckpt4, key1) == value4);
    testutil_assert(table->get(ckpt4, key2) == value2);
    testutil_assert(table->get(ckpt4, key3) == value3);

    /* Test illegal update behaviors. */
    database.set_stable_timestamp(60);
    txn1 = database.begin_transaction();
    testutil_check(table->insert(txn1, key1, value1));
    model_testutil_assert_exception(txn1->prepare(60), model::wiredtiger_abort_exception);
    txn1->rollback();

    txn1 = database.begin_transaction();
    testutil_check(table->insert(txn1, key1, value1));
    txn1->prepare(62);
    database.set_stable_timestamp(62);
    model_testutil_assert_exception(txn1->commit(60, 62), model::wiredtiger_abort_exception);
    txn1->rollback();
}

/*
 * test_checkpoint_wt --
 *     The basic test of the checkpoint model, also in WiredTiger.
 */
static void
test_checkpoint_wt(void)
{
    model::kv_database database;
    model::kv_table_ptr table = database.create_table("table");

    /* Keys. */
    const model::data_value key1("Key 1");
    const model::data_value key2("Key 2");
    const model::data_value key3("Key 3");
    const model::data_value key4("Key 4");
    const model::data_value key5("Key 5");

    /* Values. */
    const model::data_value value1("Value 1");
    const model::data_value value2("Value 2");
    const model::data_value value3("Value 3");
    const model::data_value value4("Value 4");
    const model::data_value value5("Value 5");

    /* Transactions. */
    model::kv_transaction_ptr txn1, txn2;

    /* Create the test's home directory and database. */
    WT_CONNECTION *conn;
    WT_SESSION *session;
    WT_SESSION *session1;
    WT_SESSION *session2;
    const char *uri = "table:table";

    testutil_recreate_dir(home);
    testutil_wiredtiger_open(opts, home, ENV_CONFIG, nullptr, &conn, false, false);
    testutil_check(conn->open_session(conn, nullptr, nullptr, &session));
    testutil_check(conn->open_session(conn, nullptr, nullptr, &session1));
    testutil_check(conn->open_session(conn, nullptr, nullptr, &session2));
    testutil_check(
      session->create(session, uri, "key_format=S,value_format=S,log=(enabled=false)"));

    testutil_assert(database.stable_timestamp() == wt_get_stable_timestamp(conn));

    /* Add some data. */
    wt_model_txn_begin_both(txn1, session1);
    wt_model_txn_insert_both(table, uri, txn1, session1, key1, value1);
    wt_model_txn_commit_both(txn1, session1, 10);
    wt_model_txn_begin_both(txn1, session1);
    wt_model_txn_insert_both(table, uri, txn1, session1, key2, value2);
    wt_model_txn_commit_both(txn1, session1, 20);

    /* Create a named checkpoint. */
    wt_model_ckpt_create_both("ckpt1");

    /* Set the stable timestamp and create an unnamed checkpoint. */
    wt_model_set_stable_timestamp_both(15);
    wt_model_ckpt_create_both();

    /* Add more data. */
    wt_model_txn_begin_both(txn1, session1);
    wt_model_txn_insert_both(table, uri, txn1, session1, key3, value3);
    wt_model_txn_commit_both(txn1, session1, 30);

    /* Verify that we have the data that we expect. */
    wt_model_ckpt_assert(table, uri, "ckpt1", key1);
    wt_model_ckpt_assert(table, uri, "ckpt1", key2);
    wt_model_ckpt_assert(table, uri, "ckpt1", key3);
    wt_model_ckpt_assert(table, uri, nullptr, key1);
    wt_model_ckpt_assert(table, uri, nullptr, key2);
    wt_model_ckpt_assert(table, uri, nullptr, key3);

    /* Verify that we have the data that we expect - with read timestamps. */
    wt_model_ckpt_assert(table, uri, "ckpt1", key1, 15);
    wt_model_ckpt_assert(table, uri, "ckpt1", key2, 15);
    wt_model_ckpt_assert(table, uri, "ckpt1", key3, 15);

    /* Add two more keys; check that only that committed data are included. */
    wt_model_txn_begin_both(txn1, session1);
    wt_model_txn_begin_both(txn2, session2);
    wt_model_txn_insert_both(table, uri, txn1, session1, key4, value4);
    wt_model_txn_insert_both(table, uri, txn2, session2, key5, value5);
    wt_model_txn_commit_both(txn1, session1, 40);
    wt_model_set_stable_timestamp_both(40);
    wt_model_ckpt_create_both("ckpt2");
    wt_model_ckpt_assert(table, uri, "ckpt2", key3);
    wt_model_ckpt_assert(table, uri, "ckpt2", key4);
    wt_model_ckpt_assert(table, uri, "ckpt2", key5);
    wt_model_txn_commit_both(txn2, session2, 50);

    /* Test with prepared transactions. */
    wt_model_txn_begin_both(txn1, session1);
    wt_model_txn_begin_both(txn2, session2);
    wt_model_txn_insert_both(table, uri, txn1, session1, key1, value4);
    wt_model_txn_insert_both(table, uri, txn2, session2, key2, value5);
    wt_model_txn_prepare_both(txn1, session1, 55);
    wt_model_txn_prepare_both(txn2, session2, 55);
    wt_model_txn_commit_both(txn1, session1, 60, 60);
    wt_model_txn_commit_both(txn2, session2, 60, 65);
    wt_model_set_stable_timestamp_both(60);
    wt_model_ckpt_create_both("ckpt3");
    wt_model_ckpt_assert(table, uri, "ckpt3", key1);
    wt_model_ckpt_assert(table, uri, "ckpt3", key2);
    wt_model_ckpt_assert(table, uri, "ckpt3", key3);

    /* Test moving the stable timestamp backwards - this should fail silently. */
    wt_model_set_stable_timestamp_both(50);
    testutil_assert(database.stable_timestamp() == wt_get_stable_timestamp(conn));
    wt_model_ckpt_create_both("ckpt4");
    wt_model_ckpt_assert(table, uri, "ckpt4", key1);
    wt_model_ckpt_assert(table, uri, "ckpt4", key2);
    wt_model_ckpt_assert(table, uri, "ckpt4", key3);

    /* Verify. */
    testutil_assert(table->verify_noexcept(conn));

    /* Clean up. */
    testutil_check(session->close(session, nullptr));
    testutil_check(session1->close(session1, nullptr));
    testutil_check(session2->close(session2, nullptr));
    testutil_check(conn->close(conn, nullptr));
}

/*
 * usage --
 *     Print usage help for the program.
 */
static void
usage(void)
{
    fprintf(stderr, "usage: %s%s\n", progname, opts->usage);
    exit(EXIT_FAILURE);
}

/*
 * main --
 *     The main entry point for the test.
 */
int
main(int argc, char *argv[])
{
    int ch;
    WT_DECL_RET;

    (void)testutil_set_progname(argv);

    opts = &_opts;
    memset(opts, 0, sizeof(*opts));

    /*
     * Parse the command-line arguments.
     */
    testutil_parse_begin_opt(argc, argv, SHARED_PARSE_OPTIONS, opts);
    while ((ch = __wt_getopt(progname, argc, argv, SHARED_PARSE_OPTIONS)) != EOF)
        switch (ch) {
        default:
            if (testutil_parse_single_opt(opts, ch) != 0)
                usage();
        }
    argc -= __wt_optind;
    if (argc != 0)
        usage();

    testutil_parse_end_opt(opts);
    testutil_work_dir_from_path(home, sizeof(home), opts->home);

    /*
     * Tests.
     */
    try {
        ret = EXIT_SUCCESS;
        test_checkpoint();
        test_checkpoint_wt();
    } catch (std::exception &e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        ret = EXIT_FAILURE;
    }

    /*
     * Clean up.
     */
    /* Delete the work directory. */
    if (!opts->preserve)
        testutil_remove(home);

    testutil_cleanup(opts);
    return ret;
}
