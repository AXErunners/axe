// Copyright (c) 2018-2020 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef AXE_EVODB_H
#define AXE_EVODB_H

#include "dbwrapper.h"
#include "sync.h"
#include "uint256.h"

// "b_b" was used in the initial version of deterministic MN storage
// "b_b2" was used after compact diffs were introduced
static const std::string EVODB_BEST_BLOCK = "b_b2";

class CEvoDB
{
private:
    CCriticalSection cs;
    CDBWrapper db;

    typedef CDBTransaction<CDBWrapper, CDBBatch> RootTransaction;
    typedef CDBTransaction<RootTransaction, RootTransaction> CurTransaction;
    typedef CScopedDBTransaction<RootTransaction, RootTransaction> ScopedTransaction;

    CDBBatch rootBatch;
    RootTransaction rootDBTransaction;
    CurTransaction curDBTransaction;

public:
    CEvoDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    std::unique_ptr<ScopedTransaction> BeginTransaction()
    {
        LOCK(cs);
        auto t = ScopedTransaction::Begin(curDBTransaction);
        return t;
    }

    CurTransaction& GetCurTransaction()
    {
        return curDBTransaction;
    }

    template <typename K, typename V>
    bool Read(const K& key, V& value)
    {
        LOCK(cs);
        return curDBTransaction.Read(key, value);
    }

    template <typename K, typename V>
    void Write(const K& key, const V& value)
    {
        LOCK(cs);
        curDBTransaction.Write(key, value);
    }

    template <typename K>
    bool Exists(const K& key)
    {
        LOCK(cs);
        return curDBTransaction.Exists(key);
    }

    template <typename K>
    void Erase(const K& key)
    {
        LOCK(cs);
        curDBTransaction.Erase(key);
    }

    CDBWrapper& GetRawDB()
    {
        return db;
    }

    size_t GetMemoryUsage()
    {
        return rootDBTransaction.GetMemoryUsage();
    }

    bool CommitRootTransaction();

    bool VerifyBestBlock(const uint256& hash);
    void WriteBestBlock(const uint256& hash);
};

extern CEvoDB* evoDb;

#endif //AXE_EVODB_H
