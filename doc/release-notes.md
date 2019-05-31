Axe Core version 1.4.1.0
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
using version < 0.13 you will have to reindex (start with -reindex-chainstate
or -reindex) to make sure your wallet has all the new data synced. Upgrading from
version 0.13 should not require any additional actions.

Downgrade warning
-----------------

### Downgrade to a version < 1.3.0.0

Downgrading to a version smaller than 0.13 is not supported anymore as DIP2/DIP3 has
activated on mainnet and testnet.

### Downgrade to versions 1.3.0.0

Downgrading to 1.3 releases is fully supported until DIP0008 activation but is not
recommended unless you have some serious issues with version 1.4.

Notable changes
===============

Fixed governance votes pruning for invalid masternodes
------------------------------------------------------
A community member reported a possible attack that involves DoSing masternodes to force the network
to prune all governance votes from this masternodes. This could be used to manipulate vote outcomes.

This vulnerability is currently not possible to execute as LLMQ DKGs and PoSe have not activated yet on
mainnet. This version includes a fix that requires to have at least 51% masternodes to upgrade to
1.4.0.1, after which superblock trigger voting will automatically fix the discrepancies between
old and new nodes. This also means that we will postpone activation of LLMQ DKGs and thus PoSe until
at least 51% of masternodes have upgraded to 1.4.0.1.

Fixed a rare memory/db leak in LLMQ based InstantSend
-----------------------------------------------------
We fixed a rare memory/db leak in LLMQ based InstantSend leak which would only occur when reorganizations
would happen.
