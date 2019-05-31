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

DIP0004 - Coinbase Payload v2
-----------------------------
Coinbase Payload v2 introduces new field `merkleRootQuorums` which represents the merkle root of
all the hashes of the final quorum commitments of all active LLMQ sets. This allows SPV clients
to verify active LLMQ sets and use this information to further verify ChainLocks and LLMQ-based
InstantSend messages. Coinbase Payload v2 relies on DIP0008 (bit 4) activation.

https://github.com/axerunners/dips/blob/master/dip-0004.md#calculating-the-merkle-root-of-the-active-llmqs

DIP0008 - ChainLocks
--------------------
This version introduces ChainLocks, a technology for near-instant confirmation of blocks and
finding near-instant consensus on the longest valid/accepted chain. ChainLocks leverages LLMQ
Signing Requests/Sessions to accomplish this. ChainLocks relies on DIP0008 (bit 4) activation and
`SPORK_19_CHAINLOCKS_ENABLED` spork.

Read more: https://github.com/axerunners/dips/blob/master/dip-0008.md

DIP0010 - LLMQ-based InstantSend
--------------------------------
InstantSend is a feature to allow instant confirmations of payments. It works by locking transaction
inputs through masternode quorums. It has been present in Axe for a few years and been proven to work.
Nevertheless, there are some limits which could theoretically be removed in the old system but doing so
would have created risks in terms of scalability and security.

We introduce LLMQ-based InstantSend which is designed to be much more scalable without sacrificing
security and which allows all transactions to be treated as InstantSend transactions. The old system
differentiated transactions as InstantSend transactions by using the P2P message “ix” instead of “tx”.
Since this distinction is not required in the new system, the P2P message “ix” will be removed after
DIP0008 deployment (for now, transactions sent via "ix" message will be relayed further via "tx" message).

Read more: https://github.com/axerunners/dips/blob/master/dip-0010.md

Network
------
Legacy messages `mnw`, `mnwb`, `mnget`, `mnb`, `mnp`, `dseg`, `mnv`, `qdcommit` and their corresponding
inventory types (7, 10, 14, 15, 19, 22) are no longer suported.

Message `version` is extended with a 256 bit field - a challenge sent to a masternode. Masternode which
received such a challenge must reply with new p2p message `mnauth` directly after `verack`. This `mnauth`
message must include a signed challenge that was previously sent via `version`.

Mining
------
Due to changes in coinbase payload this version requires for miners to signal their readiness via
BIP9-like mechanism - by setting bit 4 of the block version to 1. Note that if your mining software
simply uses `coinbase_payload` field from `getblocktemplate` RPC and doesn't construct coinbase payload
manually then there should be no changes to your mining software required. We however encourage pools
and solo-miners to check their software compatibility on testnet to ensure flawless migration.

PrivateSend
-----------
The wallet will try to create and consume denoms a bit more accurately now. It will also only create a
limited number of inputs for each denominated amount to prevent bloating itself with mostly the smallest
denoms. You can control this number of inputs via new `-privatesenddenoms` cmd-line option (default is 300).

InstantSend
-----------
Legacy InstantSend is going to be superseded by the newly implemented LLMQ-based one once DIP0008 (bit 4)
is active and `SPORK_20_INSTANTSEND_LLMQ_BASED` spork is ON.

Sporks
------
There are two new sporks introduced in this version - `SPORK_19_CHAINLOCKS_ENABLED` and
`SPORK_20_INSTANTSEND_LLMQ_BASED`. `SPORK_17_QUORUM_DKG_ENABLED` was introduced in v0.13 but was kept OFF.
It will be turned on once 80% masternodes are upgraded to v1.4 which will enable DKG and DKG-based PoSe.

QR codes
--------
Wallet can now show QR codes for addresses in the address book, receiving addresses and addresses identified
in transactions list (right click -> "Show QR-code").

RPC changes
-----------
There are a few changes in existing RPC interfaces in this release:
- for blockchain based RPC commands `instantlock` will say `true` if the transaction
was locked via LLMQ based ChainLocks (for backwards compatibility reasons)
- `prioritisetransaction` no longer allows adjusting priority
- `getgovernanceinfo` no longer has `masternodewatchdogmaxseconds` and `sentinelpingmaxseconds` fields
- `masternodelist` no longer supports `activeseconds`, `daemon`, `lastseen`, `protocol`, `keyid`, `rank`
and `sentinel` modes, new mode - `pubkeyoperator`
- `masternode count` no longer has `ps_compatible` and `qualify` fields and `ps` and `qualify` modes
- `masternode winner` and `masternode current` no longer have `protocol`, `lastseen` and `activeseconds`
fields, new field - `proTxHash`
- `debug` supports new categories: `chainlocks`, `llmq`, `llmq-dkg`, `llmq-sigs`
- `mnsync` no longer has `IsMasternodeListSynced` and `IsWinnersListSynced` fields
- various RPCs that had `instantlock` field have `chainlock` (excluding mempool RPCs) and
`instantlock_internal` fields now

There are also new RPC commands:
- `bls fromsecret` parses a BLS secret key and returns the secret/public key pair
- `quorum` is a set of commands for quorums/LLMQs e.g. `list` to show active quorums (by default,
can specify different `count`) or `info` to shows detailed information about some specific quorum
etc., see `help quorum`

Few RPC commands are no longer supported: `estimatepriority`, `estimatesmartpriority`,
`gobject getvotes`, `masternode start-*`, `masternode genkey`, `masternode list-conf`, `masternode check`,
`masternodebroadcast`, `sentinelping`

See `help command` in rpc for more info.

ZMQ changes
-----------
Added two new messages `hashchainlock` and `rawchainlock` which return the hash of the chainlocked block
or the raw block itself respectively.

Command-line options
--------------------

Changes in existing cmd-line options:
- `-bip9params` supports optional `window` and `threshold` values now

New cmd-line options:
- `-watchquorums`
- `-privatesenddenoms`
- `-dip3params` (regtest-only)
- `-llmqchainlocks` (devnet-only)

Few cmd-line options are no longer supported: `-limitfreerelay`, `-relaypriority`, `-blockprioritysize`,
`-sendfreetransactions`, `-mnconf`, `-mnconflock`, `-masternodeprivkey`

See `Help -> Command-line options` in Qt wallet or `axed --help` for more info.
