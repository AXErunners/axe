Axe Core version 1.3.0.0
==========================

Release is now available from:

  <https://github.com/AXErunners/axe/releases>

This is a new minor version release, bringing various bugfixes and other improvements.

Please report bugs using the issue tracker at github:

  <https://github.com/axerunners/axe/issues>


Upgrading and downgrading
=========================

How to Upgrade
--------------

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over /Applications/Axe-Qt (on Mac) or
axed/axe-qt (on Linux). If you upgrade after DIP0003 activation you will
have to reindex (start with -reindex-chainstate or -reindex) to make sure
your wallet has all the new data synced (only if you were using version < 1.2).

As spork15 has been activated on mainnet, there is no need for `masternode start`
anymore. Upgrading a masternode now only involves replacing binaries and restarting
the node.

Notable changes
===============

Instantsend Autolocks activated.

Number of false-positives from anti virus software should be reduced
--------------------------------------------------------------------
We have removed all mining code from Windows and Mac binaries, which should avoid most of the false-positive alerts
from anti virus software. Linux builds are not affected. The mining code found in `axe-qt` and `axed` are only meant
for regression/integration tests and devnets, so there is no harm in removing this code from non-linux builds.

Fixed an issue with invalid merkle blocks causing SPV nodes to ban other nodes
------------------------------------------------------------------------------
A fix that was introduces in the last minor version caused creation of invalid merkle blocks, which in turn cause SPV
nodes to ban 1.2.3 nodes. This can be observed on mobile clients which have troubles maintaining connections. This
release fixes this issue and should allow SPV/mobile clients to sync with upgraded nodes.

Hardened spork15 value to 213696
---------------------------------
We've hardened the spork15 block height to 213696, which makes sure that syncing from scratch will always work, no
matter if spork15 is received in-time or not.

Bug fixes/Other improvements
----------------------------
There are few bug fixes in this release:
- Fixed an issue with transaction sometimes not being fully zapped when `-zapwallettxes` is used
- Fixed an issue with the `protx revoke` RPC and REASON_CHANGE_OF_KEYS
