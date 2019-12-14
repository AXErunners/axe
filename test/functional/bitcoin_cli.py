#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test dash-cli"""
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

class TestBitcoinCli(BitcoinTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        """Main test logic"""

        self.log.info("Compare responses from gewalletinfo RPC and `dash-cli getwalletinfo`")
        cli_get_info = self.nodes[0].cli.getwalletinfo()
        rpc_get_info = self.nodes[0].getwalletinfo()

        assert_equal(cli_get_info, rpc_get_info)

        self.log.info("Compare responses from getblockchaininfo RPC and `dash-cli getblockchaininfo`")
        cli_get_info = self.nodes[0].cli.getblockchaininfo()
        rpc_get_info = self.nodes[0].getblockchaininfo()

        assert_equal(cli_get_info, rpc_get_info)

if __name__ == '__main__':
    TestBitcoinCli().main()
