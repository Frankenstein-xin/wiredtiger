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

#include <iomanip>
#include <sstream>

#include "wiredtiger.h"
extern "C" {
#include "test_util.h"
}

#include "model/test/wiredtiger_util.h"
#include "model/util.h"

/*
 * wt_get --
 *     Read from WiredTiger.
 */
model::data_value
wt_get(
  WT_SESSION *session, const char *uri, const model::data_value &key, model::timestamp_t timestamp)
{
    WT_CURSOR *cursor;
    WT_DECL_RET;
    char cfg[64];
    model::data_value out;

    if (timestamp == 0)
        testutil_check(session->begin_transaction(session, nullptr));
    else {
        testutil_snprintf(cfg, sizeof(cfg), "read_timestamp=%" PRIx64, timestamp);
        testutil_check(session->begin_transaction(session, cfg));
    }
    testutil_check(session->open_cursor(session, uri, nullptr, nullptr, &cursor));

    model::set_wt_cursor_key(cursor, key);
    ret = cursor->search(cursor);
    if (ret != WT_NOTFOUND && ret != WT_ROLLBACK)
        testutil_check(ret);
    if (ret == 0)
        out = model::get_wt_cursor_value(cursor);

    testutil_check(cursor->close(cursor));
    testutil_check(session->commit_transaction(session, nullptr));
    return ret == 0 ? out : model::NONE;
}

/*
 * wt_get_ext --
 *     Read from WiredTiger, but also return the error code.
 */
int
wt_get_ext(WT_SESSION *session, const char *uri, const model::data_value &key,
  model::data_value &out, model::timestamp_t timestamp)
{
    WT_CURSOR *cursor;
    WT_DECL_RET;
    char cfg[64];

    if (timestamp == 0)
        testutil_check(session->begin_transaction(session, nullptr));
    else {
        testutil_snprintf(cfg, sizeof(cfg), "read_timestamp=%" PRIx64, timestamp);
        testutil_check(session->begin_transaction(session, cfg));
    }
    testutil_check(session->open_cursor(session, uri, nullptr, nullptr, &cursor));

    model::set_wt_cursor_key(cursor, key);
    ret = cursor->search(cursor);
    if (ret != WT_NOTFOUND && ret != WT_ROLLBACK && ret != WT_PREPARE_CONFLICT)
        testutil_check(ret);
    if (ret == 0)
        out = model::get_wt_cursor_value(cursor);
    else
        out = model::NONE;

    testutil_check(cursor->close(cursor));
    testutil_check(session->commit_transaction(session, nullptr));
    return ret;
}

/*
 * wt_insert --
 *     Write to WiredTiger.
 */
int
wt_insert(WT_SESSION *session, const char *uri, const model::data_value &key,
  const model::data_value &value, model::timestamp_t timestamp, bool overwrite)
{
    WT_CURSOR *cursor;
    WT_DECL_RET;
    char cfg[64];

    testutil_check(session->begin_transaction(session, nullptr));
    testutil_check(session->open_cursor(
      session, uri, nullptr, overwrite ? nullptr : "overwrite=false", &cursor));

    model::set_wt_cursor_key(cursor, key);
    model::set_wt_cursor_value(cursor, value);
    ret = cursor->insert(cursor);
    if (ret != WT_DUPLICATE_KEY && ret != WT_ROLLBACK)
        testutil_check(ret);

    testutil_check(cursor->close(cursor));
    if (timestamp == 0)
        cfg[0] = '\0';
    else
        testutil_snprintf(cfg, sizeof(cfg), "commit_timestamp=%" PRIx64, timestamp);
    testutil_check(session->commit_transaction(session, cfg));

    return ret;
}

/*
 * wt_remove --
 *     Delete from WiredTiger.
 */
int
wt_remove(
  WT_SESSION *session, const char *uri, const model::data_value &key, model::timestamp_t timestamp)
{
    WT_CURSOR *cursor;
    WT_DECL_RET;
    char cfg[64];

    testutil_check(session->begin_transaction(session, nullptr));
    testutil_check(session->open_cursor(session, uri, nullptr, nullptr, &cursor));

    model::set_wt_cursor_key(cursor, key);
    ret = cursor->remove(cursor);
    if (ret != WT_NOTFOUND && ret != WT_ROLLBACK)
        testutil_check(ret);

    testutil_check(cursor->close(cursor));
    if (timestamp == 0)
        cfg[0] = '\0';
    else
        testutil_snprintf(cfg, sizeof(cfg), "commit_timestamp=%" PRIx64, timestamp);
    testutil_check(session->commit_transaction(session, cfg));

    return ret;
}

/*
 * wt_update --
 *     Update a key in WiredTiger.
 */
int
wt_update(WT_SESSION *session, const char *uri, const model::data_value &key,
  const model::data_value &value, model::timestamp_t timestamp, bool overwrite)
{
    WT_CURSOR *cursor;
    WT_DECL_RET;
    char cfg[64];

    testutil_check(session->begin_transaction(session, nullptr));
    testutil_check(session->open_cursor(
      session, uri, nullptr, overwrite ? nullptr : "overwrite=false", &cursor));

    model::set_wt_cursor_key(cursor, key);
    model::set_wt_cursor_value(cursor, value);
    ret = cursor->update(cursor);
    if (ret != WT_NOTFOUND && ret != WT_ROLLBACK)
        testutil_check(ret);

    testutil_check(cursor->close(cursor));
    if (timestamp == 0)
        cfg[0] = '\0';
    else
        testutil_snprintf(cfg, sizeof(cfg), "commit_timestamp=%" PRIx64, timestamp);
    testutil_check(session->commit_transaction(session, cfg));

    return ret;
}

/*
 * wt_txn_begin --
 *     Begin a transaction.
 */
void
wt_txn_begin(WT_SESSION *session, model::timestamp_t read_timestamp)
{
    char cfg[64];
    if (read_timestamp == model::k_timestamp_latest)
        testutil_check(session->begin_transaction(session, nullptr));
    else {
        testutil_snprintf(cfg, sizeof(cfg), "read_timestamp=%" PRIx64, read_timestamp);
        testutil_check(session->begin_transaction(session, cfg));
    }
}

/*
 * wt_txn_commit --
 *     Commit a transaction.
 */
void
wt_txn_commit(
  WT_SESSION *session, model::timestamp_t commit_timestamp, model::timestamp_t durable_timestamp)
{
    char cfg[64];
    if (commit_timestamp == model::k_timestamp_none) {
        testutil_assert(durable_timestamp == model::k_timestamp_none);
        testutil_check(session->commit_transaction(session, nullptr));
    } else if (durable_timestamp == model::k_timestamp_none) {
        testutil_snprintf(cfg, sizeof(cfg), "commit_timestamp=%" PRIx64, commit_timestamp);
        testutil_check(session->commit_transaction(session, cfg));
    } else {
        testutil_snprintf(cfg, sizeof(cfg),
          "commit_timestamp=%" PRIx64 ",durable_timestamp=%" PRIx64, commit_timestamp,
          durable_timestamp);
        testutil_check(session->commit_transaction(session, cfg));
    }
}

/*
 * wt_txn_prepare --
 *     Prepare a transaction.
 */
void
wt_txn_prepare(WT_SESSION *session, model::timestamp_t prepare_timestamp)
{
    char cfg[64];
    testutil_snprintf(cfg, sizeof(cfg), "prepare_timestamp=%" PRIx64, prepare_timestamp);
    testutil_check(session->prepare_transaction(session, cfg));
}

/*
 * wt_txn_reset_snapshot --
 *     Reset the transaction snapshot.
 */
void
wt_txn_reset_snapshot(WT_SESSION *session)
{
    testutil_check(session->reset_snapshot(session));
}

/*
 * wt_txn_rollback --
 *     Roll back a transaction.
 */
void
wt_txn_rollback(WT_SESSION *session)
{
    testutil_check(session->rollback_transaction(session, nullptr));
}

/*
 * wt_txn_set_commit_timestamp --
 *     Set the commit timestamp for all subsequent updates.
 */
void
wt_txn_set_commit_timestamp(WT_SESSION *session, model::timestamp_t commit_timestamp)
{
    char cfg[64];
    testutil_snprintf(cfg, sizeof(cfg), "commit_timestamp=%" PRIx64, commit_timestamp);
    testutil_check(session->timestamp_transaction(session, cfg));
}

/*
 * wt_txn_get --
 *     Read from WiredTiger.
 */
model::data_value
wt_txn_get(WT_SESSION *session, const char *uri, const model::data_value &key)
{
    WT_CURSOR *cursor;
    WT_DECL_RET;
    const char *value;

    value = nullptr;

    testutil_check(session->open_cursor(session, uri, nullptr, nullptr, &cursor));
    model::set_wt_cursor_key(cursor, key);
    testutil_check_error_ok(ret = cursor->search(cursor), WT_NOTFOUND);
    if (ret == 0)
        testutil_check(cursor->get_value(cursor, &value));

    testutil_check(cursor->close(cursor));
    return ret == 0 ? model::data_value(value) : model::NONE;
}

/*
 * wt_txn_insert --
 *     Write to WiredTiger.
 */
int
wt_txn_insert(WT_SESSION *session, const char *uri, const model::data_value &key,
  const model::data_value &value, bool overwrite)
{
    WT_CURSOR *cursor;
    WT_DECL_RET;

    testutil_check(session->open_cursor(
      session, uri, nullptr, overwrite ? nullptr : "overwrite=false", &cursor));
    ret = model::wt_cursor_insert(cursor, key, value);
    testutil_check(cursor->close(cursor));
    return ret;
}

/*
 * wt_ckpt_get --
 *     Read from WiredTiger.
 */
model::data_value
wt_ckpt_get(WT_SESSION *session, const char *uri, const model::data_value &key,
  const char *ckpt_name, model::timestamp_t debug_read_timestamp)
{
    WT_CURSOR *cursor;
    WT_DECL_RET;
    size_t config_len;
    const char *value;
    char buf[64];
    char *config;

    value = nullptr;

    if (ckpt_name == nullptr)
        ckpt_name = WT_CHECKPOINT;
    config_len = 64 + strlen(ckpt_name);
    config = (char *)alloca(config_len);

    /* Set the checkpoint name. */
    testutil_snprintf(config, config_len, "checkpoint=%s", ckpt_name);

    /* Set the checkpoint debug read timestamp, if set. */
    if (debug_read_timestamp != model::k_timestamp_none) {
        testutil_snprintf(
          buf, sizeof(buf), ",debug=(checkpoint_read_timestamp=%" PRIx64 ")", debug_read_timestamp);
        testutil_check(__wt_strcat(config, config_len, buf));
    }

    testutil_check(session->open_cursor(session, uri, nullptr, config, &cursor));

    model::set_wt_cursor_key(cursor, key);
    ret = cursor->search(cursor);
    if (ret != WT_NOTFOUND && ret != WT_ROLLBACK)
        testutil_check(ret);
    if (ret == 0)
        testutil_check(cursor->get_value(cursor, &value));

    model::data_value r = ret == 0 ? model::data_value(value) : model::NONE;
    testutil_check(cursor->close(cursor));
    return r;
}

/*
 * wt_ckpt_create --
 *     Create a WiredTiger checkpoint.
 */
void
wt_ckpt_create(WT_SESSION *session, const char *ckpt_name)
{
    size_t config_len;
    char *config;

    if (ckpt_name == nullptr)
        config = nullptr;
    else {
        config_len = 64 + strlen(ckpt_name);
        config = (char *)alloca(config_len);
        testutil_snprintf(config, config_len, "name=%s", ckpt_name);
    }

    testutil_check(session->checkpoint(session, config));
}

/*
 * wt_get_stable_timestamp --
 *     Get the stable timestamp in WiredTiger.
 */
model::timestamp_t wt_get_stable_timestamp(WT_CONNECTION *conn)
{
    char buf[64];
    testutil_check(conn->query_timestamp(conn, buf, "get=stable_timestamp"));

    std::istringstream ss(buf);
    model::timestamp_t t;
    ss >> std::hex >> t;
    return t;
}

/*
 * wt_set_stable_timestamp --
 *     Set the stable timestamp in WiredTiger.
 */
void
wt_set_stable_timestamp(WT_CONNECTION *conn, model::timestamp_t timestamp)
{
    char buf[64];

    testutil_snprintf(buf, sizeof(buf), "stable_timestamp=%" PRIx64, timestamp);
    testutil_check(conn->set_timestamp(conn, buf));
}

/*
 * wt_print_debug_log --
 *     Print the contents of a debug log to a file.
 */
void
wt_print_debug_log(WT_CONNECTION *conn, const char *file)
{
    int ret;
    WT_SESSION *session;
    ret = conn->open_session(conn, nullptr, nullptr, &session);
    if (ret != 0)
        throw model::wiredtiger_exception("Cannot open a session: ", ret);
    model::wiredtiger_session_guard session_guard(session);

    WT_LSN start_lsn;
    WT_ASSIGN_LSN(&start_lsn, &((WT_CONNECTION_IMPL *)conn)->log->first_lsn);
    ret = __wt_txn_printlog(session, file, WT_TXN_PRINTLOG_UNREDACT, &start_lsn, nullptr);
    if (ret != 0)
        throw model::wiredtiger_exception("Cannot print the debug log: ", ret);
}
