#!/usr/bin/env python3
# Copyright (c) 2014-2015 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test timestampindex generation and fetching
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *


class TimestampIndexTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 4

    def setup_network(self):
        self.nodes = []
        # Nodes 0/1 are "wallet" nodes
        self.nodes.append(start_node(0, self.options.tmpdir))
        self.nodes.append(start_node(1, self.options.tmpdir, ["-timestampindex"]))
        # Nodes 2/3 are used for testing
        self.nodes.append(start_node(2, self.options.tmpdir))
        self.nodes.append(start_node(3, self.options.tmpdir, ["-timestampindex"]))
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[0], 2)
        connect_nodes(self.nodes[0], 3)

        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        self.log.info("Mining 5 blocks...")
        blockhashes = self.nodes[0].generate(5)
        low = self.nodes[0].getblock(blockhashes[0])["time"]
        high = self.nodes[0].getblock(blockhashes[4])["time"]
        self.sync_all()
        self.log.info("Checking timestamp index...")
        hashes = self.nodes[1].getblockhashes(high, low)
        assert_equal(len(hashes), 5)
        assert_equal(sorted(blockhashes), sorted(hashes))
        self.log.info("Passed")


if __name__ == '__main__':
    TimestampIndexTest().main()
