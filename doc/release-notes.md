Axe Core version 1.3.1.0
==========================

Release is now available from:

  <https://github.com/AXErunners/axe/releases>

This is a new version release, bringing various bugfixes and other improvements.

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

Decentralized governance activated.
