// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ZMQ_ZMQPUBLISHNOTIFIER_H
#define BITCOIN_ZMQ_ZMQPUBLISHNOTIFIER_H

#include <zmq/zmqabstractnotifier.h>

class CBlockIndex;
class CGovernanceVote;
class CGovernanceObject;

class CZMQAbstractPublishNotifier : public CZMQAbstractNotifier
{
private:
    uint32_t nSequence; //!< upcounting per message sequence number

public:

    /* send zmq multipart message
       parts:
          * command
          * data
          * message sequence number
    */
    bool SendMessage(const char *command, const void* data, size_t size);

    bool Initialize(void *pcontext) override;
    void Shutdown() override;
};

class CZMQPublishHashBlockNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyBlock(const CBlockIndex *pindex) override;
};

class CZMQPublishHashChainLockNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyChainLock(const CBlockIndex *pindex, const llmq::CChainLockSig& clsig) override;
};

class CZMQPublishHashTransactionNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyTransaction(const CTransaction &transaction) override;
};

class CZMQPublishHashTransactionLockNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyTransactionLock(const CTransaction &transaction, const llmq::CInstantSendLock& islock) override;
};

class CZMQPublishHashGovernanceVoteNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyGovernanceVote(const CGovernanceVote &vote) override;
};

class CZMQPublishHashGovernanceObjectNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyGovernanceObject(const CGovernanceObject &object) override;
};

class CZMQPublishHashInstantSendDoubleSpendNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyInstantSendDoubleSpendAttempt(const CTransaction &currentTx, const CTransaction &previousTx) override;
};

class CZMQPublishRawBlockNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyBlock(const CBlockIndex *pindex) override;
};

class CZMQPublishRawChainLockNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyChainLock(const CBlockIndex *pindex, const llmq::CChainLockSig& clsig) override;
};

class CZMQPublishRawChainLockSigNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyChainLock(const CBlockIndex *pindex, const llmq::CChainLockSig& clsig) override;
};

class CZMQPublishRawTransactionNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyTransaction(const CTransaction &transaction) override;
};

class CZMQPublishRawTransactionLockNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyTransactionLock(const CTransaction &transaction, const llmq::CInstantSendLock& islock) override;
};

class CZMQPublishRawTransactionLockSigNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyTransactionLock(const CTransaction &transaction, const llmq::CInstantSendLock& islock) override;
};

class CZMQPublishRawGovernanceVoteNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyGovernanceVote(const CGovernanceVote &vote) override;
};

class CZMQPublishRawGovernanceObjectNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyGovernanceObject(const CGovernanceObject &object) override;
};

class CZMQPublishRawInstantSendDoubleSpendNotifier : public CZMQAbstractPublishNotifier
{
public:
    bool NotifyInstantSendDoubleSpendAttempt(const CTransaction &currentTx, const CTransaction &previousTx) override;
};
#endif // BITCOIN_ZMQ_ZMQPUBLISHNOTIFIER_H
