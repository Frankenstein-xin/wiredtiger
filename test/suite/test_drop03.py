#!/usr/bin/env python
#
# Public Domain 2014-present MongoDB, Inc.
# Public Domain 2008-2014 WiredTiger, Inc.
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

import wttest

# test_drop03.py
#    Test dropping a collection under an active transaction. We should return EBUSY - WT-10576
class test_drop03(wttest.WiredTigerTestCase):
    uri = 'table:test_drop03'

    def test_drop_during_txn(self):
        self.session.create(self.uri, 'key_format=S,value_format=S')
        cursor = self.session.open_cursor(self.uri, None)
        self.session.begin_transaction()
        cursor['key: aaa'] = 'value: aaa'
        cursor['key: aab'] = 'value: aab'
        cursor['key: aac'] = 'value: aac'
        cursor['key: aad'] = 'value: aad'
        cursor['key: bbb'] = 'value: bbb'
        cursor.close()
        # Drop call should fail with EBUSY with or without the force option.
        self.assertTrue(self.raisesBusy(lambda: self.session.drop(self.uri, "force=false")),
            "was expecting drop call to fail with EBUSY")
        self.assertTrue(self.raisesBusy(lambda: self.session.drop(self.uri, "force=true")),
            "was expecting drop call to fail with EBUSY")
        self.session.commit_transaction()
        self.session.close()

if __name__ == '__main__':
    wttest.run()