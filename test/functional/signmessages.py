#!/usr/bin/env python3
# Copyright (c) 2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test RPC commands for signing and verifying messages."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

class SignMessagesTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 1

    def setup_network(self, split=False):
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir)
        self.is_network_split = False

    def run_test(self):
        message = 'This is just a test message'

        # Test the signing with a privkey
        privKey = "cU4zhap7nPJAWeMFu4j6jLrfPmqakDAzy8zn8Fhb3oEevdm4e5Lc"
        address = "yeMpGzMj3rhtnz48XsfpB8itPHhHtgxLc3"
        signature = self.nodes[0].signmessagewithprivkey(privKey, message)

        # Verify the message
        assert(self.nodes[0].verifymessage(address, signature, message))

        # Test the signing with an address with wallet
        address = self.nodes[0].getnewaddress()
        signature = self.nodes[0].signmessage(address, message)

        # Verify the message
        assert(self.nodes[0].verifymessage(address, signature, message))

if __name__ == '__main__':
    SignMessagesTest().main()
