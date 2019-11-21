#!/usr/bin/env python3
# Copyright (c) 2018 The Axe Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.mininode import *
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from time import *

'''
'''

class SporkTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 3
        self.setup_clean_chain = True
        self.extra_args = [["-sporkkey=cP4EKFyJsHT39LDqgdcB43Y3YXjNyjb5Fuas1GQSeAtjnZWmZEQK"], [], []]

    def setup_network(self):
        disable_mocktime()
        self.setup_nodes()
        # connect only 2 first nodes at start
        connect_nodes(self.nodes[0], 1)

    def get_test_spork_state(self, node):
        info = node.spork('active')
        # use InstantSend spork for tests
        return info['SPORK_2_INSTANTSEND_ENABLED']

    def set_test_spork_state(self, node, state):
        if state:
            value = 0
        else:
            value = 4070908800
        # use InstantSend spork for tests
        node.spork('SPORK_2_INSTANTSEND_ENABLED', value)

    def run_test(self):
        # check test spork default state
        assert(not self.get_test_spork_state(self.nodes[0]))
        assert(not self.get_test_spork_state(self.nodes[1]))
        assert(not self.get_test_spork_state(self.nodes[2]))

        # check spork propagation for connected nodes
        self.set_test_spork_state(self.nodes[0], True)
        start = time()
        sent = False
        while True:
            if self.get_test_spork_state(self.nodes[1]):
                sent = True
                break
            if time() > start + 10:
                break
            sleep(0.1)
        assert(sent)

        # restart nodes to check spork persistence
        self.stop_node(0)
        self.stop_node(1)
        self.nodes[0] = self.start_node(0, self.options.tmpdir)
        self.nodes[1] = self.start_node(1, self.options.tmpdir)
        assert(self.get_test_spork_state(self.nodes[0]))
        assert(self.get_test_spork_state(self.nodes[1]))

        # Force finish mnsync node as otherwise it will never send out headers to other peers
        wait_to_sync(self.nodes[1], fast_mnsync=True)

        # Generate one block to kick off masternode sync, which also starts sporks syncing for node2
        self.nodes[1].generate(1)

        # connect new node and check spork propagation after restoring from cache
        connect_nodes(self.nodes[1], 2)
        start = time()
        sent = False
        while True:
            if self.get_test_spork_state(self.nodes[2]):
                sent = True
                break
            if time() > start + 10:
                break
            sleep(0.1)
        assert(sent)


if __name__ == '__main__':
    SporkTest().main()
