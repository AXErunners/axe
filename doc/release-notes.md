Dash Core version 1.1.5
==========================

Release is now available from:

  <https://axerunners.com>

This is a new minor version release, bringing various bugfixes and other
improvements.

Please report bugs using the issue tracker at github:

  <https://github.com/axerunners/axe/issues>

Notable changes
===============

Improved initial sync
--------------------

Some users had problems getting their nodes synced. The issue occurred due to nodes trying to
get additional data from each available peer but not being able to process this data fast enough.
This was recognized as a stalled sync process and thus the process was reset. To address the issue
we limited sync process to 3 peers max now and the issue should no longer appear as long as there
are at least 4 connections.

Testnet/Devnet fixes
--------------------

Turned out that a low-diff rule for slow blocks backported from Bitcoin works a bit too aggressive for
a blockchain which uses a dynamic per-block difficulty adjustment algorithm (DGW). While blocks are still
produced at a more or less constant rate on average, the rate however is way too high.

We also lifted multiple ports restriction on devnet and also included other fixes which should improve
connectivity on devnets which are using nodes with multiple different ports.
