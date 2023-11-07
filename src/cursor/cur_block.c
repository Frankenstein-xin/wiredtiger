/*-
 * Copyright (c) 2014-present MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __curblock_next_raw_n_walk --
 *     walk the tree to fill the key value pairs.
 */
static int
__curblock_next_raw_n_walk(
  WT_SESSION_IMPL *session, WT_CURSOR *cursor, WT_ITEM **keys, WT_ITEM **values, size_t *n)
{
    WT_CURSOR_BLOCK *cblock;
    WT_CURSOR_BTREE *cbt;
    WT_DECL_RET;
    size_t count;

    cblock = (WT_CURSOR_BLOCK *)cursor;
    cbt = (WT_CURSOR_BTREE *)cursor;
    count = 0;

    F_CLR(cursor, WT_CURSTD_BLOCK_COPY_KEY);
    cbt->upd_value->buf = &cblock->values[0];
    WT_ERR(__wt_btcur_next(cbt, false));
    if (F_ISSET(cursor, WT_CURSTD_BLOCK_COPY_KEY))
        WT_ERR(__wt_buf_set(session, &cblock->keys[0], cursor->key.data, cursor->key.size));
    count++;

    /* Ignore not found error from this point. */

    for (; count < MAX_BLOCK_ITEM; ++count) {
        F_CLR(cursor, WT_CURSTD_BLOCK_COPY_KEY);
        cbt->upd_value->buf = &cblock->values[count];
        ret = __wt_btcur_next_on_page(cbt);
        if (ret == WT_NOTFOUND || ret == WT_PREPARE_CONFLICT) {
            ret = 0;
            break;
        }
        WT_ERR(ret);
        if (F_ISSET(cursor, WT_CURSTD_BLOCK_COPY_KEY))
            WT_ERR(__wt_buf_set(session, &cblock->keys[count], cursor->key.data, cursor->key.size));
    }

    *keys = cblock->keys;
    *values = cblock->values;
    *n = count;
err:
    cbt->upd_value->buf = &cbt->upd_value->_buf;
    F_CLR(cursor, WT_CURSTD_KEY_SET);
    F_CLR(cursor, WT_CURSTD_VALUE_SET);
    return (ret);
}

/*
 * __curblock_next_raw_n --
 *     next_raw_n implementation for block cursor
 */
static int
__curblock_next_raw_n(WT_CURSOR *cursor, WT_ITEM **keys, WT_ITEM **values, size_t *n)
{
    WT_CURSOR_BTREE *cbt;
    WT_DECL_RET;
    WT_SESSION_IMPL *session;

    cbt = (WT_CURSOR_BTREE *)cursor;
    CURSOR_API_CALL(cursor, session, next_raw_n, CUR2BT(cbt));

    WT_ERR(__cursor_copy_release(cursor));

    WT_ERR(__curfile_check_cbt_txn(session, cbt));

    WT_WITH_CHECKPOINT(
      session, cbt, ret = __curblock_next_raw_n_walk(session, cursor, keys, values, n));
    WT_ERR(ret);

    /* The call maintains a position. */
    WT_ASSERT(session, F_ISSET(cbt, WT_CBT_ACTIVE));

err:
    API_END_RET(session, ret);
}

/*
 * __curblock_prev_raw_n_walk --
 *     walk the tree to fill the key value pairs.
 */
static int
__curblock_prev_raw_n_walk(
  WT_SESSION_IMPL *session, WT_CURSOR *cursor, WT_ITEM **keys, WT_ITEM **values, size_t *n)
{
    WT_CURSOR_BLOCK *cblock;
    WT_CURSOR_BTREE *cbt;
    WT_DECL_RET;
    size_t count;

    cblock = (WT_CURSOR_BLOCK *)cursor;
    cbt = (WT_CURSOR_BTREE *)cursor;
    count = 0;

    F_CLR(cursor, WT_CURSTD_BLOCK_COPY_KEY);
    cbt->upd_value->buf = &cblock->values[0];
    WT_ERR(__wt_btcur_prev(cbt, false));
    if (F_ISSET(cursor, WT_CURSTD_BLOCK_COPY_KEY))
        WT_ERR(__wt_buf_set(session, &cblock->keys[0], cursor->key.data, cursor->key.size));
    count++;

    for (; count < MAX_BLOCK_ITEM; ++count) {
        F_CLR(cursor, WT_CURSTD_BLOCK_COPY_KEY);
        cbt->upd_value->buf = &cblock->values[count];
        ret = __wt_btcur_prev_on_page(cbt);
        if (ret == WT_NOTFOUND || ret == WT_PREPARE_CONFLICT) {
            ret = 0;
            break;
        }
        WT_ERR(ret);
        if (F_ISSET(cursor, WT_CURSTD_BLOCK_COPY_KEY))
            WT_ERR(__wt_buf_set(session, &cblock->keys[count], cursor->key.data, cursor->key.size));
    }

    *keys = cblock->keys;
    *values = cblock->values;
    *n = count;
err:
    cbt->upd_value->buf = &cbt->upd_value->_buf;
    F_CLR(cursor, WT_CURSTD_KEY_SET);
    F_CLR(cursor, WT_CURSTD_VALUE_SET);
    return (ret);
}

/*
 * __curblock_prev_raw_n --
 *     prev_raw_n implementation for block cursor
 */
static int
__curblock_prev_raw_n(WT_CURSOR *cursor, WT_ITEM **keys, WT_ITEM **values, size_t *n)
{
    WT_CURSOR_BTREE *cbt;
    WT_DECL_RET;
    WT_SESSION_IMPL *session;

    cbt = (WT_CURSOR_BTREE *)cursor;
    CURSOR_API_CALL(cursor, session, next_raw_n, CUR2BT(cbt));

    WT_ERR(__cursor_copy_release(cursor));

    WT_ERR(__curfile_check_cbt_txn(session, cbt));

    WT_WITH_CHECKPOINT(
      session, cbt, ret = __curblock_prev_raw_n_walk(session, cursor, keys, values, n));
    WT_ERR(ret);

    /* The call maintains a position. */
    WT_ASSERT(session, F_ISSET(cbt, WT_CBT_ACTIVE));

err:
    API_END_RET(session, ret);
}

/*
 * __wt_curblock_init --
 *     Initialize a block cursor.
 */
int
__wt_curblock_init(WT_SESSION_IMPL *session, WT_CURSOR_BLOCK *cblock)
{
    WT_CURSOR *cursor;
    WT_CURSOR_BTREE *cbt;
    WT_DECL_RET;

    cursor = &cblock->cbt.iface;
    cbt = &cblock->cbt;

    if (CUR2BT(cbt)->type != BTREE_ROW)
        WT_ERR_MSG(session, EINVAL, "block cursor only supports row store");

    if (!WT_STREQ(cursor->key_format, "u") || !WT_STREQ(cursor->value_format, "u"))
        WT_ERR_MSG(session, EINVAL, "block cursor only supports raw format");

    cursor->next_raw_n = __curblock_next_raw_n;
    cursor->prev_raw_n = __curblock_prev_raw_n;

    WT_CLEAR(cblock->keys);
    WT_CLEAR(cblock->values);

err:
    return (ret);
}

/*
 * __wt_curblock_close --
 *     Close a block cursor.
 */
void
__wt_curblock_close(WT_SESSION_IMPL *session, WT_CURSOR_BLOCK *cblock)
{
    size_t i;

    for (i = 0; i < MAX_BLOCK_ITEM; i++) {
        __wt_buf_free(session, &cblock->keys[i]);
        __wt_buf_free(session, &cblock->values[i]);
    }
}
