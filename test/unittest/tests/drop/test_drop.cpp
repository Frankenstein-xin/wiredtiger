/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include <thread>
#include <catch2/catch.hpp>
#include <iostream>
#include "wiredtiger.h"
#include "wt_internal.h"
#include "../utils.h"
#include "../wrappers/connection_wrapper.h"
#include "../wrappers/item_wrapper.h"

static int
insert_key_value(WT_CURSOR *cursor, const char *key, const char *value)
{
    item_wrapper item_key(key);
    item_wrapper item_value(value);
    __wt_cursor_set_raw_key(cursor, item_key.get_item());
    __wt_cursor_set_raw_value(cursor, item_value.get_item());
    return cursor->insert(cursor);
}

static void
insert_sample_values(WT_CURSOR *cursor)
{
    REQUIRE(insert_key_value(cursor, "key1", "value1") == 0);
    REQUIRE(insert_key_value(cursor, "key2", "value2") == 0);
    REQUIRE(insert_key_value(cursor, "key3", "value3") == 0);
    REQUIRE(insert_key_value(cursor, "key4", "value4") == 0);
    REQUIRE(insert_key_value(cursor, "key5", "value5") == 0);
}


/*
 * print_dhandles
 *     For diagnostics of any failing tests, prints the dhandles on a session.
 */
static void
print_dhandles(WT_SESSION_IMPL *session_impl)
{
    WT_CONNECTION_IMPL *conn;
    WT_DATA_HANDLE *dhandle;

    printf("Session 0x%p, dhandle: 0x%p\n", session_impl, session_impl->dhandle);
    conn = S2C(session_impl);

    TAILQ_FOREACH (dhandle, &conn->dhqh, q) {
        printf(".   dhandle 0x%p, session_inuse %d, session_ref %u\n",
          dhandle, dhandle->session_inuse, dhandle->session_ref);
    }
}

/*
 * check_txn_updates
 *     For diagnostics of any failing tests, prints information about mod values in a txn.
 */
static bool
check_txn_updates(std::string const &label, WT_SESSION_IMPL *session_impl, bool diagnostics)
{
    bool ok = true;

    if (diagnostics) {
        WT_TXN *txn = session_impl->txn;

        printf("check_txn_updates() - %s\n", label.c_str());
        print_dhandles(session_impl);
        printf("  txn = 0x%p, txn->id = 0x%" PRIu64 ", txn->mod = 0x%p, txn->mod_count = %u\n", txn,
          txn->id, txn->mod, txn->mod_count);

        WT_TXN_OP *op = txn->mod;
        for (u_int i = 0; i < txn->mod_count; i++, op++) {
            switch (op->type) {
            case WT_TXN_OP_NONE:
            case WT_TXN_OP_REF_DELETE:
            case WT_TXN_OP_TRUNCATE_COL:
            case WT_TXN_OP_TRUNCATE_ROW:
                break;
            case WT_TXN_OP_BASIC_COL:
            case WT_TXN_OP_BASIC_ROW:
            case WT_TXN_OP_INMEM_COL:
            case WT_TXN_OP_INMEM_ROW:
                WT_UPDATE *upd = op->u.op_upd;
                printf(".   mod %u, upd 0x%p, op->type = %i, upd->txnid = 0x%" PRIx64 ", upd->durable_ts 0x%" PRIu64 "\n",
                  i, upd, op->type, upd->txnid, upd->durable_ts);

                // At least during current diagnosis a txnid greater than 100 means something has gone wrong
                if (upd->txnid > 100) {
                    printf(".     The upd->txnid value is wierd!\n");
                }

                break;
            }
        }
    }

    return ok;
}

/*
 * report_cache_status
 *     For diagnostics of any failing tests, prints cache information.
 */
// static void
// report_cache_status(WT_CACHE *cache, std::string const &label, bool diagnostics)
// {
//     if (diagnostics) {
//         printf("Cache (label is '%s'):\n", label.c_str());
//         printf(". pages_inmem:      %" PRIu64 "\n", cache->pages_inmem);
//         printf(". pages_evicted:    %" PRIu64 "\n", cache->pages_evicted);
//         printf(". bytes_image_intl: %" PRIu64 "\n", cache->bytes_image_intl);
//         printf(". bytes_image_leaf: %" PRIu64 "\n", cache->bytes_image_leaf);
//         printf(". pages_dirty_intl: %" PRIu64 "\n", cache->pages_dirty_intl);
//         printf(". pages_dirty_leaf: %" PRIu64 "\n", cache->pages_dirty_leaf);
//         printf(". bytes_dirty_intl: %" PRIu64 "\n", cache->bytes_dirty_intl);
//         printf(". bytes_dirty_leaf: %" PRIu64 "\n", cache->bytes_dirty_leaf);
//     }
// }

int
debug_dropped_state(WT_SESSION_IMPL *session, const char *uri)
{
    WT_CONNECTION_IMPL *conn;
    WT_DATA_HANDLE *dhandle;

    printf("Starting debug_dropped_state()\n");

    conn = S2C(session);

    WT_ASSERT(session, FLD_ISSET(session->lock_flags, WT_SESSION_LOCKED_HANDLE_LIST_WRITE));
    WT_ASSERT(session, session->dhandle == NULL);

    TAILQ_FOREACH (dhandle, &conn->dhqh, q) {
        printf(".   dhandle 0c%p, name %s, is dropped %d, is open %d, flags 0x%x, type %d\n",
          dhandle, dhandle->name, F_ISSET(dhandle, WT_DHANDLE_DROPPED), F_ISSET(dhandle, WT_DHANDLE_OPEN), dhandle->flags, dhandle->type);

        if (dhandle->type == __wt_data_handle::WT_DHANDLE_TYPE_BTREE) {
            WT_BTREE *btree = (WT_BTREE*)dhandle->handle;
            printf(".     btree = 0x%p, btree flags = 0x%x, root.page 0x%p\n",
              btree, btree->flags, btree->root.page);
        }

        if (strcmp(uri, dhandle->name) == 0) {
            //            F_CLR(dhandle, WT_DHANDLE_DROPPED);
        }
    }

    return 0;
}

void
lock_and_debug_dropped_state(WT_SESSION_IMPL *session, const char *uri) {
    WT_WITH_HANDLE_LIST_WRITE_LOCK(
      session, debug_dropped_state(session, uri));
}


/*
 * cache_destroy_memory_check --
 *     A simple test displays cache usage info as it runs.
 */
// static void
// cache_destroy_memory_check(
//   std::string const &config, int expected_open_cursor_result, bool diagnostics)
// {
//     SECTION("Check memory freed when using a cursor: config = " + config)
//     {
//         ConnectionWrapper conn(DB_HOME);
//         WT_SESSION_IMPL *session_impl = conn.createSession();
//         WT_SESSION *session = &session_impl->iface;
//         std::string uri = "table:cursor_test";

//         report_cache_status(conn.getWtConnectionImpl()->cache, ", created connection", diagnostics);

//         REQUIRE(session->create(session, uri.c_str(), "key_format=S,value_format=S") == 0);
//         report_cache_status(
//           conn.getWtConnectionImpl()->cache, config + ", created session", diagnostics);

//         REQUIRE(session->begin_transaction(session, "") == 0);
//         report_cache_status(
//           conn.getWtConnectionImpl()->cache, config + ", begun transaction", diagnostics);

//         WT_CURSOR *cursor = nullptr;
//         int open_cursor_result =
//           session->open_cursor(session, uri.c_str(), nullptr, config.c_str(), &cursor);
//         REQUIRE(open_cursor_result == expected_open_cursor_result);

//         if (open_cursor_result == 0) {
//             report_cache_status(
//               conn.getWtConnectionImpl()->cache, config + ", opened cursor", diagnostics);

//             insert_sample_values(cursor);
//             report_cache_status(
//               conn.getWtConnectionImpl()->cache, config + ", inserted values", diagnostics);

//             REQUIRE(cursor->close(cursor) == 0);
//             report_cache_status(
//               conn.getWtConnectionImpl()->cache, config + ", closed cursor", diagnostics);

//             REQUIRE(session->commit_transaction(session, "") == 0);
//             report_cache_status(
//               conn.getWtConnectionImpl()->cache, config + ", committed transaction", diagnostics);
//         }
//     }
// }



int64_t
get_stats_value(WT_CURSOR *stats_cursor, int stat)
{
    int64_t dhandles_open_count = 0;
    char *desc = nullptr;
    char *pvalue = nullptr;

    stats_cursor->set_key(stats_cursor, stat);
    REQUIRE(stats_cursor->search(stats_cursor) == 0);
    REQUIRE(stats_cursor->get_value(stats_cursor, &desc, &pvalue, &dhandles_open_count) == 0);
    return dhandles_open_count;
}


int64_t
get_dhandles_open_count(WT_CURSOR *stats_cursor)
{
    return get_stats_value(stats_cursor, WT_STAT_CONN_DH_CONN_HANDLE_COUNT);
}

// void
// dump_stats(WT_CURSOR *stats_cursor)
// {
//     printf("Dump Stats:\n");
//     printf(". WT_STAT_CONN_DH_CONN_HANDLE_SIZE value = %" PRIu64 "\n",
//       get_stats_value(stats_cursor, WT_STAT_CONN_DH_CONN_HANDLE_SIZE));
//     printf(". WT_STAT_CONN_DH_CONN_HANDLE_COUNT value = %" PRIu64 "\n",
//       get_stats_value(stats_cursor, WT_STAT_CONN_DH_CONN_HANDLE_COUNT));
//     printf(". WT_STAT_CONN_DH_SWEEP_REF value = %" PRIu64 "\n",
//       get_stats_value(stats_cursor, WT_STAT_CONN_DH_SWEEP_REF));
//     printf(". WT_STAT_CONN_DH_SWEEP_CLOSE value = %" PRIu64 "\n",
//       get_stats_value(stats_cursor, WT_STAT_CONN_DH_SWEEP_CLOSE));
//     printf(". WT_STAT_CONN_DH_SWEEP_REMOVE value = %" PRIu64 "\n",
//       get_stats_value(stats_cursor, WT_STAT_CONN_DH_SWEEP_REMOVE));
//     printf(". WT_STAT_CONN_DH_SWEEP_TOD value = %" PRIu64 "\n",
//       get_stats_value(stats_cursor, WT_STAT_CONN_DH_SWEEP_TOD));
//     printf(". WT_STAT_CONN_DH_SWEEPS value = %" PRIu64 "\n",
//       get_stats_value(stats_cursor, WT_STAT_CONN_DH_SWEEPS));
// }


/*
 * thread_function_drop --
 *     This function is designed to be used as a thread function, and force drops a table.
 */
static void
thread_function_drop(WT_SESSION *session, std::string const &uri)
{
    session->drop(session, uri.c_str(), "force=true");
}


/*
 * thread_function_drop_in_session --
 *     This function is designed to be used as a thread function, and force drops a table.
 */
static void
thread_function_drop_in_session(WT_CONNECTION *connection, std::string const &cfg, std::string const &uri)
{
    printf("Starting thread_function_drop_in_session()\n");
    WT_SESSION *session;
    REQUIRE(connection->open_session(connection, nullptr, cfg.c_str(), &session) == 0);
    REQUIRE(session->drop(session, uri.c_str(), "force=true") == 0);
    REQUIRE(session->close(session, "") == 0);
    printf("Ending thread_function_drop_in_session()\n");
}


/*
 * drop_test --
 *     Perform a series of combinations of drop operations to confirm correct behavior
 *     in each case.
 */
static void
drop_test(std::string const &config, bool transaction,
  int expected_commit_result, bool diagnostics)
{
    ConnectionWrapper conn(DB_HOME);
    WT_SESSION_IMPL *session_impl = conn.createSession();
    WT_SESSION *session = &session_impl->iface;
    std::string uri = "table:cursor_test";
    std::string file_uri = "file:cursor_test.wt";

    REQUIRE(session->create(session, uri.c_str(), "key_format=S,value_format=S") == 0);

    if (transaction) {
        REQUIRE(session->begin_transaction(session, "") == 0);
    }

    WT_CURSOR *cursor = nullptr;
    REQUIRE(session->open_cursor(session, uri.c_str(), nullptr, config.c_str(), &cursor) == 0);
    insert_sample_values(cursor);

    std::string txn_as_string = transaction ? "true" : "false";

    SECTION(
      "Drop in one thread: transaction = " + txn_as_string + ", config = " + config)
    {
        check_txn_updates("before close", session_impl, diagnostics);
        REQUIRE(cursor->close(cursor) == 0);
        check_txn_updates("before drop", session_impl, diagnostics);
        lock_and_debug_dropped_state(session_impl, file_uri.c_str());
        __wt_sleep(1, 0);
        REQUIRE(session->drop(session, uri.c_str(), "force=true") == 0);

        if (diagnostics)
            printf("After drop\n");

        __wt_sleep(1, 0);
        if (S2C(session)->sweep_cond != NULL)
            __wt_cond_signal(session_impl, S2C(session_impl)->sweep_cond);
        __wt_sleep(1, 0);

        lock_and_debug_dropped_state(session_impl, file_uri.c_str());

        if (transaction) {
            __wt_sleep(1, 0);
            if (S2C(session)->sweep_cond != NULL)
                __wt_cond_signal(session_impl, S2C(session_impl)->sweep_cond);
            __wt_sleep(1, 0);

            check_txn_updates("before checkpoint", session_impl, diagnostics);
            REQUIRE(session->checkpoint(session, nullptr) == EINVAL);

            __wt_sleep(1, 0);
            if (S2C(session)->sweep_cond != NULL)
                __wt_cond_signal(session_impl, S2C(session_impl)->sweep_cond);
            __wt_sleep(1, 0);

            check_txn_updates("before commit", session_impl, diagnostics);

            REQUIRE(session->commit_transaction(session, "") == expected_commit_result);
            check_txn_updates("after commit", session_impl, diagnostics);

            __wt_sleep(1, 0);
            if (S2C(session)->sweep_cond != NULL)
                __wt_cond_signal(session_impl, S2C(session_impl)->sweep_cond);
            __wt_sleep(5, 0);

            check_txn_updates("near the end", session_impl, diagnostics);

            REQUIRE(session->close(session, "") == 0);
        }

        printf("Completed a test\n");
    }

    SECTION(
      "Drop in second session: transaction = " + txn_as_string + ", config = " + config)
    {
        printf("In drop_test(): session 0x%p\n", (void*)session_impl);

        check_txn_updates("before close", session_impl, diagnostics);
        REQUIRE(cursor->close(cursor) == 0);
        check_txn_updates("before drop", session_impl, diagnostics);
        lock_and_debug_dropped_state(session_impl, file_uri.c_str());
        __wt_sleep(1, 0);

        std::thread thread([&]() { thread_function_drop_in_session(conn.getWtConnection(), "", uri); });
        thread.join();

        if (diagnostics)
            printf("After drop\n");

        __wt_sleep(1, 0);
        if (S2C(session)->sweep_cond != NULL)
            __wt_cond_signal(session_impl, S2C(session_impl)->sweep_cond);
        __wt_sleep(1, 0);

        lock_and_debug_dropped_state(session_impl, file_uri.c_str());

        if (transaction) {
            __wt_sleep(1, 0);
            if (S2C(session)->sweep_cond != NULL)
                __wt_cond_signal(session_impl, S2C(session_impl)->sweep_cond);
            __wt_sleep(1, 0);

            check_txn_updates("before checkpoint", session_impl, diagnostics);
            REQUIRE(session->checkpoint(session, nullptr) == EINVAL);

            __wt_sleep(1, 0);
            if (S2C(session)->sweep_cond != NULL)
                __wt_cond_signal(session_impl, S2C(session_impl)->sweep_cond);
            __wt_sleep(1, 0);

            check_txn_updates("before commit", session_impl, diagnostics);

            REQUIRE(session->commit_transaction(session, "") == expected_commit_result);
            check_txn_updates("after commit", session_impl, diagnostics);

            __wt_sleep(1, 0);
            if (S2C(session)->sweep_cond != NULL)
                __wt_cond_signal(session_impl, S2C(session_impl)->sweep_cond);
            __wt_sleep(5, 0);

            check_txn_updates("near the end", session_impl, diagnostics);

            REQUIRE(session->close(session, "") == 0);
        }

        printf("Completed a test\n");
    }
}

/*
 * multiple_drop_test --
 *     Ensure that a series of create/force drop operations on a table work as expected.
 */
static void
multiple_drop_test(std::string const &config, int expected_open_cursor_result,
  int expected_commit_result, bool do_sleep, bool diagnostics)
{
    ConnectionWrapper conn(DB_HOME);
    std::string uri = "table:cursor_test";
    std::string sleep_as_string = do_sleep ? "true" : "false";

    SECTION("Multiple drop test: config = " + config + ", sleep = " + sleep_as_string)
    {
        int count = 0;
        const int limit = 5;

        while (count < limit) {
            count++;

            WT_SESSION_IMPL *session_impl = conn.createSession();
            WT_SESSION *session = &session_impl->iface;

            REQUIRE(session->create(session, uri.c_str(), "key_format=S,value_format=S") == 0);
            REQUIRE(session->begin_transaction(session, "") == 0);

            WT_CURSOR *cursor = nullptr;
            int open_cursor_result =
              session->open_cursor(session, uri.c_str(), nullptr, config.c_str(), &cursor);
            REQUIRE(open_cursor_result == expected_open_cursor_result);

            if (open_cursor_result == 0) {
                insert_sample_values(cursor);

                check_txn_updates("before close", session_impl, diagnostics);
                REQUIRE(cursor->close(cursor) == 0);

                if (diagnostics)
                    printf("After close\n");

                if (do_sleep)
                    __wt_sleep(1, 0);
            }

            check_txn_updates("before drop", session_impl, diagnostics);
            REQUIRE(session->drop(session, uri.c_str(), "force=true") == 0);

            if (diagnostics)
                printf("After drop\n");

            if (do_sleep)
                __wt_sleep(1, 0);

            check_txn_updates("before checkpoint", session_impl, diagnostics);
            REQUIRE(session->checkpoint(session, nullptr) == EINVAL);

            if (do_sleep)
                __wt_sleep(1, 0);

            check_txn_updates("before commit", session_impl, diagnostics);
            REQUIRE(session->commit_transaction(session, "") == expected_commit_result);
            check_txn_updates("after commit", session_impl, diagnostics);
            REQUIRE(session->close(session, nullptr) == 0);
        }

        // Confirm the correct number of loops were executed & we didn't exit early for any reason
        REQUIRE(count == limit);
    }
}

TEST_CASE("Drop: dropped dhandles", "[drop]")
{
    const bool diagnostics = true;

    drop_test("", true, EINVAL, diagnostics);
    drop_test("", false, 0, diagnostics);

    return;

    multiple_drop_test("", 0, EINVAL, false, diagnostics);
    multiple_drop_test("", 0, EINVAL, true, diagnostics);
}