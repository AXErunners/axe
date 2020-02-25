Axe Core version 1.5.0.1
==========================

Release is now available from:

  <https://github.com/AXErunners/axe/releases>

This is a new major version release, bringing new features, various bugfixes and other improvements.

Please report bugs using the issue tracker at github:

  <https://github.com/axerunners/axe/issues>


Upgrading and downgrading
=========================

How to Upgrade
--------------

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over /Applications/Axe-Qt (on Mac) or
axed/axe-qt (on Linux). If you upgrade after DIP0003 activation and you were
using version < 1.4 you will have to reindex (start with -reindex-chainstate
or -reindex) to make sure your wallet has all the new data synced. Upgrading from
version 1.4.1 should not require any additional actions.

Downgrade warning
-----------------

### Downgrade to a version < 1.4.0.0

Downgrading to a version smaller than 1.4 is not supported anymore.

Notable changes
===============

Upstream backports

Qt updates

Testnet restarted.
