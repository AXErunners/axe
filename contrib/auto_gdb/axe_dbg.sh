#!/bin/bash
# use testnet settings,  if you need mainnet,  use ~/.axecore/axed.pid file instead
export LC_ALL=C

axe_pid=$(<~/.axecore/testnet3/axed.pid)
sudo gdb -batch -ex "source debug.gdb" axed ${axe_pid}
