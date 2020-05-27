// Copyright (c) 2014-2020 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <privatesend/privatesend-client.h>

#include <consensus/validation.h>
#include <core_io.h>
#include <init.h>
#include <masternode/masternode-payments.h>
#include <masternode/masternode-sync.h>
#include <masternode/masternode-meta.h>
#include <netmessagemaker.h>
#include <script/sign.h>
#include <txmempool.h>
#include <util.h>
#include <utilmoneystr.h>
#include <validation.h>
#include <wallet/coincontrol.h>

#include <memory>
#include <univalue.h>

CPrivateSendClientManager privateSendClient;

void CPrivateSendClientManager::ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman)
{
    if (fMasternodeMode) return;
    if (!fEnablePrivateSend) return;
    if (!masternodeSync.IsBlockchainSynced()) return;

    if (!CheckDiskSpace()) {
        ResetPool();
        fPrivateSendRunning = false;
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::ProcessMessage -- Not enough disk space, disabling PrivateSend.\n");
        return;
    }

    if (strCommand == NetMsgType::DSQUEUE) {
        if (pfrom->nVersion < MIN_PRIVATESEND_PEER_PROTO_VERSION) {
            LogPrint(BCLog::PRIVATESEND, "DSQUEUE -- peer=%d using obsolete version %i\n", pfrom->GetId(), pfrom->nVersion);
            if (g_enable_bip61) {
                connman.PushMessage(pfrom, CNetMsgMaker(pfrom->GetSendVersion()).Make(NetMsgType::REJECT, strCommand,
                                                                                      REJECT_OBSOLETE, strprintf(
                                "Version must be %d or greater", MIN_PRIVATESEND_PEER_PROTO_VERSION)));
            }
            return;
        }

        CPrivateSendQueue dsq;
        vRecv >> dsq;

        {
            TRY_LOCK(cs_vecqueue, lockRecv);
            if (!lockRecv) return;

            // process every dsq only once
            for (const auto& q : vecPrivateSendQueue) {
                if (q == dsq) {
                    return;
                }
                if (q.fReady == dsq.fReady && q.masternodeOutpoint == dsq.masternodeOutpoint) {
                    // no way the same mn can send another dsq with the same readiness this soon
                    LogPrint(BCLog::PRIVATESEND, "DSQUEUE -- Peer %s is sending WAY too many dsq messages for a masternode with collateral %s\n", pfrom->GetLogString(), dsq.masternodeOutpoint.ToStringShort());
                    return;
                }
            }
        } // cs_vecqueue

        LogPrint(BCLog::PRIVATESEND, "DSQUEUE -- %s new\n", dsq.ToString());

        if (dsq.IsTimeOutOfBounds()) return;

        auto mnList = deterministicMNManager->GetListAtChainTip();
        auto dmn = mnList.GetValidMNByCollateral(dsq.masternodeOutpoint);
        if (!dmn) return;

        if (!dsq.CheckSignature(dmn->pdmnState->pubKeyOperator.Get())) {
            LOCK(cs_main);
            Misbehaving(pfrom->GetId(), 10);
            return;
        }

        // if the queue is ready, submit if we can
        if (dsq.fReady) {
            LOCK(cs_deqsessions);
            for (auto& session : deqSessions) {
                CDeterministicMNCPtr mnMixing;
                if (session.GetMixingMasternodeInfo(mnMixing) && mnMixing->pdmnState->addr == dmn->pdmnState->addr && session.GetState() == POOL_STATE_QUEUE) {
                    LogPrint(BCLog::PRIVATESEND, "DSQUEUE -- PrivateSend queue (%s) is ready on masternode %s\n", dsq.ToString(), dmn->pdmnState->addr.ToString());
                    session.SubmitDenominate(connman);
                    return;
                }
            }
        } else {
            int64_t nLastDsq = mmetaman.GetMetaInfo(dmn->proTxHash)->GetLastDsq();
            int64_t nDsqThreshold = mmetaman.GetDsqThreshold(dmn->proTxHash, mnList.GetValidMNsCount());
            LogPrint(BCLog::PRIVATESEND, "DSQUEUE -- nLastDsq: %d  nDsqThreshold: %d  nDsqCount: %d\n", nLastDsq, nDsqThreshold, mmetaman.GetDsqCount());
            // don't allow a few nodes to dominate the queuing process
            if (nLastDsq != 0 && nDsqThreshold > mmetaman.GetDsqCount()) {
                LogPrint(BCLog::PRIVATESEND, "DSQUEUE -- Masternode %s is sending too many dsq messages\n", dmn->proTxHash.ToString());
                return;
            }

            mmetaman.AllowMixing(dmn->proTxHash);

            LogPrint(BCLog::PRIVATESEND, "DSQUEUE -- new PrivateSend queue (%s) from masternode %s\n", dsq.ToString(), dmn->pdmnState->addr.ToString());

            LOCK(cs_deqsessions);
            for (auto& session : deqSessions) {
                CDeterministicMNCPtr mnMixing;
                if (session.GetMixingMasternodeInfo(mnMixing) && mnMixing->collateralOutpoint == dsq.masternodeOutpoint) {
                    dsq.fTried = true;
                }
            }

            TRY_LOCK(cs_vecqueue, lockRecv);
            if (!lockRecv) return;
            vecPrivateSendQueue.push_back(dsq);
            dsq.Relay(connman);
        }

    } else if (strCommand == NetMsgType::DSSTATUSUPDATE ||
               strCommand == NetMsgType::DSFINALTX ||
               strCommand == NetMsgType::DSCOMPLETE) {
        LOCK(cs_deqsessions);
        for (auto& session : deqSessions) {
            session.ProcessMessage(pfrom, strCommand, vRecv, connman);
        }
    }
}

void CPrivateSendClientSession::ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman)
{
    if (fMasternodeMode) return;
    if (!privateSendClient.fEnablePrivateSend) return;
    if (!masternodeSync.IsBlockchainSynced()) return;

    if (strCommand == NetMsgType::DSSTATUSUPDATE) {
        if (pfrom->nVersion < MIN_PRIVATESEND_PEER_PROTO_VERSION) {
            LogPrint(BCLog::PRIVATESEND, "DSSTATUSUPDATE -- peer=%d using obsolete version %i\n", pfrom->GetId(), pfrom->nVersion);
            if (g_enable_bip61) {
                connman.PushMessage(pfrom, CNetMsgMaker(pfrom->GetSendVersion()).Make(NetMsgType::REJECT, strCommand,
                                                                                      REJECT_OBSOLETE, strprintf(
                                "Version must be %d or greater", MIN_PRIVATESEND_PEER_PROTO_VERSION)));
            }
            return;
        }

        if (!mixingMasternode) return;
        if (mixingMasternode->pdmnState->addr != pfrom->addr) {
            return;
        }

        CPrivateSendStatusUpdate psssup;
        vRecv >> psssup;

        ProcessPoolStateUpdate(psssup);

    } else if (strCommand == NetMsgType::DSFINALTX) {
        if (pfrom->nVersion < MIN_PRIVATESEND_PEER_PROTO_VERSION) {
            LogPrint(BCLog::PRIVATESEND, "DSFINALTX -- peer=%d using obsolete version %i\n", pfrom->GetId(), pfrom->nVersion);
            if (g_enable_bip61) {
                connman.PushMessage(pfrom, CNetMsgMaker(pfrom->GetSendVersion()).Make(NetMsgType::REJECT, strCommand,
                                                                                      REJECT_OBSOLETE, strprintf(
                                "Version must be %d or greater", MIN_PRIVATESEND_PEER_PROTO_VERSION)));
            }
            return;
        }

        if (!mixingMasternode) return;
        if (mixingMasternode->pdmnState->addr != pfrom->addr) {
            return;
        }

        int nMsgSessionID;
        vRecv >> nMsgSessionID;
        CTransaction txNew(deserialize, vRecv);

        if (nSessionID != nMsgSessionID) {
            LogPrint(BCLog::PRIVATESEND, "DSFINALTX -- message doesn't match current PrivateSend session: nSessionID: %d  nMsgSessionID: %d\n", nSessionID, nMsgSessionID);
            return;
        }

        LogPrint(BCLog::PRIVATESEND, "DSFINALTX -- txNew %s", txNew.ToString());

        // check to see if input is spent already? (and probably not confirmed)
        SignFinalTransaction(txNew, pfrom, connman);

    } else if (strCommand == NetMsgType::DSCOMPLETE) {
        if (pfrom->nVersion < MIN_PRIVATESEND_PEER_PROTO_VERSION) {
            LogPrint(BCLog::PRIVATESEND, "DSCOMPLETE -- peer=%d using obsolete version %i\n", pfrom->GetId(), pfrom->nVersion);
            if (g_enable_bip61) {
                connman.PushMessage(pfrom, CNetMsgMaker(pfrom->GetSendVersion()).Make(NetMsgType::REJECT, strCommand,
                                                                                      REJECT_OBSOLETE, strprintf(
                                "Version must be %d or greater", MIN_PRIVATESEND_PEER_PROTO_VERSION)));
            }
            return;
        }

        if (!mixingMasternode) return;
        if (mixingMasternode->pdmnState->addr != pfrom->addr) {
            LogPrint(BCLog::PRIVATESEND, "DSCOMPLETE -- message doesn't match current Masternode: infoMixingMasternode=%s  addr=%s\n", mixingMasternode->pdmnState->addr.ToString(), pfrom->addr.ToString());
            return;
        }

        int nMsgSessionID;
        PoolMessage nMsgMessageID;
        vRecv >> nMsgSessionID >> nMsgMessageID;

        if (nMsgMessageID < MSG_POOL_MIN || nMsgMessageID > MSG_POOL_MAX) {
            LogPrint(BCLog::PRIVATESEND, "DSCOMPLETE -- nMsgMessageID is out of bounds: %d\n", nMsgMessageID);
            return;
        }

        if (nSessionID != nMsgSessionID) {
            LogPrint(BCLog::PRIVATESEND, "DSCOMPLETE -- message doesn't match current PrivateSend session: nSessionID: %d  nMsgSessionID: %d\n", nSessionID, nMsgSessionID);
            return;
        }

        LogPrint(BCLog::PRIVATESEND, "DSCOMPLETE -- nMsgSessionID %d  nMsgMessageID %d (%s)\n", nMsgSessionID, nMsgMessageID, CPrivateSend::GetMessageByID(nMsgMessageID));

        CompletedTransaction(nMsgMessageID);
    }
}

void CPrivateSendClientSession::ResetPool()
{
    txMyCollateral = CMutableTransaction();
    UnlockCoins();
    keyHolderStorage.ReturnAll();
    SetNull();
}

void CPrivateSendClientManager::ResetPool()
{
    LOCK(cs_deqsessions);
    nCachedLastSuccessBlock = 0;
    vecMasternodesUsed.clear();
    for (auto& session : deqSessions) {
        session.ResetPool();
    }
    deqSessions.clear();
}

void CPrivateSendClientSession::SetNull()
{
    // Client side
    mixingMasternode = nullptr;
    pendingDsaRequest = CPendingDsaRequest();

    CPrivateSendBaseSession::SetNull();
}

//
// Unlock coins after mixing fails or succeeds
//
void CPrivateSendClientSession::UnlockCoins()
{
    if (!privateSendClient.fEnablePrivateSend) return;

    while (true) {
        TRY_LOCK(GetWallets()[0]->cs_wallet, lockWallet);
        if (!lockWallet) {
            MilliSleep(50);
            continue;
        }
        for (const auto& outpoint : vecOutPointLocked)
            GetWallets()[0]->UnlockCoin(outpoint);
        break;
    }

    vecOutPointLocked.clear();
}

std::string CPrivateSendClientSession::GetStatus(bool fWaitForBlock)
{
    static int nStatusMessageProgress = 0;
    nStatusMessageProgress += 10;
    std::string strSuffix = "";

    if (fWaitForBlock || !masternodeSync.IsBlockchainSynced()) {
        return strAutoDenomResult;
    }

    switch (nState) {
    case POOL_STATE_IDLE:
        return _("PrivateSend is idle.");
    case POOL_STATE_QUEUE:
        if (nStatusMessageProgress % 70 <= 30)
            strSuffix = ".";
        else if (nStatusMessageProgress % 70 <= 50)
            strSuffix = "..";
        else if (nStatusMessageProgress % 70 <= 70)
            strSuffix = "...";
        return strprintf(_("Submitted to masternode, waiting in queue %s"), strSuffix);
    case POOL_STATE_ACCEPTING_ENTRIES:
        return strAutoDenomResult;
    case POOL_STATE_SIGNING:
        if (nStatusMessageProgress % 70 <= 40)
            return _("Found enough users, signing ...");
        else if (nStatusMessageProgress % 70 <= 50)
            strSuffix = ".";
        else if (nStatusMessageProgress % 70 <= 60)
            strSuffix = "..";
        else if (nStatusMessageProgress % 70 <= 70)
            strSuffix = "...";
        return strprintf(_("Found enough users, signing ( waiting %s )"), strSuffix);
    case POOL_STATE_ERROR:
        return _("PrivateSend request incomplete:") + " " + strLastMessage + " " + _("Will retry...");
    default:
        return strprintf(_("Unknown state: id = %u"), nState);
    }
}

std::string CPrivateSendClientManager::GetStatuses()
{
    LOCK(cs_deqsessions);
    std::string strStatus;
    bool fWaitForBlock = WaitForAnotherBlock();

    for (auto& session : deqSessions) {
        strStatus += session.GetStatus(fWaitForBlock) + "; ";
    }
    return strStatus;
}

std::string CPrivateSendClientManager::GetSessionDenoms()
{
    LOCK(cs_deqsessions);
    std::string strSessionDenoms;

    for (auto& session : deqSessions) {
        strSessionDenoms += CPrivateSend::DenominationToString(session.nSessionDenom) + "; ";
    }
    return strSessionDenoms.empty() ? "N/A" : strSessionDenoms;
}

bool CPrivateSendClientSession::GetMixingMasternodeInfo(CDeterministicMNCPtr& ret) const
{
    ret = mixingMasternode;
    return ret != nullptr;
}

bool CPrivateSendClientManager::GetMixingMasternodesInfo(std::vector<CDeterministicMNCPtr>& vecDmnsRet) const
{
    LOCK(cs_deqsessions);
    for (const auto& session : deqSessions) {
        CDeterministicMNCPtr dmn;
        if (session.GetMixingMasternodeInfo(dmn)) {
            vecDmnsRet.push_back(dmn);
        }
    }
    return !vecDmnsRet.empty();
}

//
// Check session timeouts
//
bool CPrivateSendClientSession::CheckTimeout()
{
    if (fMasternodeMode) return false;

    if (nState == POOL_STATE_IDLE) return false;

    if (nState == POOL_STATE_ERROR) {
        if (GetTime() - nTimeLastSuccessfulStep >= 10) {
            // reset after being in POOL_STATE_ERROR for 10 or more seconds
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- resetting session %d\n", __func__, nSessionID);
            SetNull();
        }
        return false;
    }

    int nLagTime = 10; // give the server a few extra seconds before resetting.
    int nTimeout = (nState == POOL_STATE_SIGNING) ? PRIVATESEND_SIGNING_TIMEOUT : PRIVATESEND_QUEUE_TIMEOUT;
    bool fTimeout = GetTime() - nTimeLastSuccessfulStep >= nTimeout + nLagTime;

    if (!fTimeout) return false;

    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- %s %d timed out (%ds)\n", __func__,
        (nState == POOL_STATE_SIGNING) ? "Signing at session" : "Session", nSessionID, nTimeout);

    SetState(POOL_STATE_ERROR);
    UnlockCoins();
    keyHolderStorage.ReturnAll();
    nTimeLastSuccessfulStep = GetTime();
    strLastMessage = CPrivateSend::GetMessageByID(ERR_SESSION);

    return true;
}

//
// Check all queues and sessions for timeouts
//
void CPrivateSendClientManager::CheckTimeout()
{
    if (fMasternodeMode) return;

    CheckQueue();

    if (!fEnablePrivateSend || !fPrivateSendRunning) return;

    LOCK(cs_deqsessions);
    for (auto& session : deqSessions) {
        if (session.CheckTimeout()) {
            strAutoDenomResult = _("Session timed out.");
        }
    }
}

//
// Execute a mixing denomination via a Masternode.
// This is only ran from clients
//
bool CPrivateSendClientSession::SendDenominate(const std::vector<std::pair<CTxDSIn, CTxOut> >& vecPSInOutPairsIn, CConnman& connman)
{
    if (fMasternodeMode) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::SendDenominate -- PrivateSend from a Masternode is not supported currently.\n");
        return false;
    }

    if (txMyCollateral == CMutableTransaction()) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClient:SendDenominate -- PrivateSend collateral not set\n");
        return false;
    }

    // we should already be connected to a Masternode
    if (!nSessionID) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::SendDenominate -- No Masternode has been selected yet.\n");
        UnlockCoins();
        keyHolderStorage.ReturnAll();
        SetNull();
        return false;
    }

    if (!CheckDiskSpace()) {
        UnlockCoins();
        keyHolderStorage.ReturnAll();
        SetNull();
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::SendDenominate -- Not enough disk space.\n");
        return false;
    }

    SetState(POOL_STATE_ACCEPTING_ENTRIES);
    strLastMessage = "";

    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::SendDenominate -- Added transaction to pool.\n");

    CMutableTransaction tx; // for debug purposes only
    std::vector<CTxDSIn> vecTxDSInTmp;
    std::vector<CTxOut> vecTxOutTmp;

    for (const auto& pair : vecPSInOutPairsIn) {
        vecTxDSInTmp.emplace_back(pair.first);
        vecTxOutTmp.emplace_back(pair.second);
        tx.vin.emplace_back(pair.first);
        tx.vout.emplace_back(pair.second);
    }

    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::SendDenominate -- Submitting partial tx %s", tx.ToString());

    // store our entry for later use
    vecEntries.emplace_back(vecTxDSInTmp, vecTxOutTmp, txMyCollateral);
    RelayIn(vecEntries.back(), connman);
    nTimeLastSuccessfulStep = GetTime();

    return true;
}

// Process incoming messages from Masternode updating the progress of mixing
void CPrivateSendClientSession::ProcessPoolStateUpdate(CPrivateSendStatusUpdate psssup)
{
    if (fMasternodeMode) return;

    // do not update state when mixing client state is one of these
    if (nState == POOL_STATE_IDLE || nState == POOL_STATE_ERROR) return;

    if (psssup.nState < POOL_STATE_MIN || psssup.nState > POOL_STATE_MAX) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- psssup.nState is out of bounds: %d\n", __func__, psssup.nState);
        return;
    }

    if (psssup.nMessageID < MSG_POOL_MIN || psssup.nMessageID > MSG_POOL_MAX) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- psssup.nMessageID is out of bounds: %d\n", __func__, psssup.nMessageID);
        return;
    }

    std::string strMessageTmp = CPrivateSend::GetMessageByID(psssup.nMessageID);
    strAutoDenomResult = _("Masternode:") + " " + strMessageTmp;

    switch (psssup.nStatusUpdate) {
        case STATUS_REJECTED: {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- rejected by Masternode: %s\n", __func__, strMessageTmp);
            SetState(POOL_STATE_ERROR);
            UnlockCoins();
            keyHolderStorage.ReturnAll();
            nTimeLastSuccessfulStep = GetTime();
            strLastMessage = strMessageTmp;
            break;
        }
        case STATUS_ACCEPTED: {
            if (nState == psssup.nState && psssup.nState == POOL_STATE_QUEUE && nSessionID == 0 && psssup.nSessionID != 0) {
                // new session id should be set only in POOL_STATE_QUEUE state
                nSessionID = psssup.nSessionID;
                nTimeLastSuccessfulStep = GetTime();
                strMessageTmp += strprintf(" Set nSessionID to %d.", nSessionID);
            }
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- accepted by Masternode: %s\n", __func__, strMessageTmp);
            break;
        }
        default: {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- psssup.nStatusUpdate is out of bounds: %d\n", __func__, psssup.nStatusUpdate);
            break;
        }
    }
}

//
// After we receive the finalized transaction from the Masternode, we must
// check it to make sure it's what we want, then sign it if we agree.
// If we refuse to sign, it's possible we'll be charged collateral
//
bool CPrivateSendClientSession::SignFinalTransaction(const CTransaction& finalTransactionNew, CNode* pnode, CConnman& connman)
{
    if (!privateSendClient.fEnablePrivateSend || !privateSendClient.fPrivateSendRunning) return false;

    if (fMasternodeMode || pnode == nullptr) return false;
    if (!mixingMasternode) return false;

    finalMutableTransaction = finalTransactionNew;
    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- finalMutableTransaction=%s", __func__, finalMutableTransaction.ToString());

    // STEP 1: check final transaction general rules

    // Make sure it's BIP69 compliant
    sort(finalMutableTransaction.vin.begin(), finalMutableTransaction.vin.end(), CompareInputBIP69());
    sort(finalMutableTransaction.vout.begin(), finalMutableTransaction.vout.end(), CompareOutputBIP69());

    if (finalMutableTransaction.GetHash() != finalTransactionNew.GetHash()) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- ERROR! Masternode %s is not BIP69 compliant!\n", __func__, mixingMasternode->proTxHash.ToString());
        UnlockCoins();
        keyHolderStorage.ReturnAll();
        SetNull();
        return false;
    }

    // Make sure all inputs/outputs are valid
    PoolMessage nMessageID{MSG_NOERR};
    if (!IsValidInOuts(finalMutableTransaction.vin, finalMutableTransaction.vout, nMessageID, nullptr)) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- ERROR! IsValidInOuts() failed: %s\n", __func__, CPrivateSend::GetMessageByID(nMessageID));
        UnlockCoins();
        keyHolderStorage.ReturnAll();
        SetNull();
        return false;
    }

    // STEP 2: make sure our own inputs/outputs are present, otherwise refuse to sign

    std::vector<CTxIn> sigs;

    for (const auto& entry : vecEntries) {
        // Check that the final transaction has all our outputs
        for (const auto& txout : entry.vecTxOut) {
            bool fFound = false;
            for (const auto& txoutFinal : finalMutableTransaction.vout) {
                if (txoutFinal == txout) {
                    fFound = true;
                    break;
                }
            }
            if (!fFound) {
                // Something went wrong and we'll refuse to sign. It's possible we'll be charged collateral. But that's
                // better than signing if the transaction doesn't look like what we wanted.
                LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- an output is missing, refusing to sign! txout=%s\n", txout.ToString());
                UnlockCoins();
                keyHolderStorage.ReturnAll();
                SetNull();
                return false;
            }
        }

        for (const auto& txdsin : entry.vecTxDSIn) {
            /* Sign my transaction and all outputs */
            int nMyInputIndex = -1;
            CScript prevPubKey = CScript();

            for (unsigned int i = 0; i < finalMutableTransaction.vin.size(); i++) {
                if (finalMutableTransaction.vin[i] == txdsin) {
                    nMyInputIndex = i;
                    prevPubKey = txdsin.prevPubKey;
                    break;
                }
            }

            if (nMyInputIndex == -1) {
                // Can't find one of my own inputs, refuse to sign. It's possible we'll be charged collateral. But that's
                // better than signing if the transaction doesn't look like what we wanted.
                LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- missing input! txdsin=%s\n", __func__, txdsin.ToString());
                UnlockCoins();
                keyHolderStorage.ReturnAll();
                SetNull();
                return false;
            }

            const CKeyStore& keystore = *GetWallets()[0];

            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- Signing my input %i\n", __func__, nMyInputIndex);
            // TODO we're using amount=0 here but we should use the correct amount. This works because Axe ignores the amount while signing/verifying (only used in Bitcoin/Segwit)
            if (!SignSignature(keystore, prevPubKey, finalMutableTransaction, nMyInputIndex, 0, int(SIGHASH_ALL | SIGHASH_ANYONECANPAY))) { // changes scriptSig
                LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- Unable to sign my own transaction!\n", __func__);
                // not sure what to do here, it will timeout...?
            }

            sigs.push_back(finalMutableTransaction.vin[nMyInputIndex]);
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- nMyInputIndex: %d, sigs.size(): %d, scriptSig=%s\n",
                    __func__, nMyInputIndex, (int)sigs.size(), ScriptToAsmStr(finalMutableTransaction.vin[nMyInputIndex].scriptSig));
        }
    }

    if (sigs.empty()) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- can't sign anything!\n", __func__);
        UnlockCoins();
        keyHolderStorage.ReturnAll();
        SetNull();

        return false;
    }

    // push all of our signatures to the Masternode
    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- pushing sigs to the masternode, finalMutableTransaction=%s", __func__, finalMutableTransaction.ToString());
    CNetMsgMaker msgMaker(pnode->GetSendVersion());
    connman.PushMessage(pnode, msgMaker.Make(NetMsgType::DSSIGNFINALTX, sigs));
    SetState(POOL_STATE_SIGNING);
    nTimeLastSuccessfulStep = GetTime();

    return true;
}

// mixing transaction was completed (failed or successful)
void CPrivateSendClientSession::CompletedTransaction(PoolMessage nMessageID)
{
    if (fMasternodeMode) return;

    if (nMessageID == MSG_SUCCESS) {
        LogPrint(BCLog::PRIVATESEND, "CompletedTransaction -- success\n");
        privateSendClient.UpdatedSuccessBlock();
        keyHolderStorage.KeepAll();
    } else {
        LogPrint(BCLog::PRIVATESEND, "CompletedTransaction -- error\n");
        keyHolderStorage.ReturnAll();
    }
    UnlockCoins();
    SetNull();
    strLastMessage = CPrivateSend::GetMessageByID(nMessageID);
}

void CPrivateSendClientManager::UpdatedSuccessBlock()
{
    if (fMasternodeMode) return;
    nCachedLastSuccessBlock = nCachedBlockHeight;
}

bool CPrivateSendClientManager::IsDenomSkipped(const CAmount& nDenomValue)
{
    return std::find(vecDenominationsSkipped.begin(), vecDenominationsSkipped.end(), nDenomValue) != vecDenominationsSkipped.end();
}

void CPrivateSendClientManager::AddSkippedDenom(const CAmount& nDenomValue)
{
    vecDenominationsSkipped.push_back(nDenomValue);
}

void CPrivateSendClientManager::RemoveSkippedDenom(const CAmount& nDenomValue)
{
    vecDenominationsSkipped.erase(std::remove(vecDenominationsSkipped.begin(), vecDenominationsSkipped.end(), nDenomValue), vecDenominationsSkipped.end());
}

bool CPrivateSendClientManager::WaitForAnotherBlock()
{
    if (!masternodeSync.IsBlockchainSynced()) return true;

    if (fPrivateSendMultiSession) return false;

    return nCachedBlockHeight - nCachedLastSuccessBlock < nMinBlocksToWait;
}

bool CPrivateSendClientManager::CheckAutomaticBackup()
{
    if (!fEnablePrivateSend || !fPrivateSendRunning) return false;

    switch (nWalletBackups) {
    case 0:
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::CheckAutomaticBackup -- Automatic backups disabled, no mixing available.\n");
        strAutoDenomResult = _("Automatic backups disabled") + ", " + _("no mixing available.");
        fPrivateSendRunning = false;               // stop mixing
        GetWallets()[0]->nKeysLeftSinceAutoBackup = 0; // no backup, no "keys since last backup"
        return false;
    case -1:
        // Automatic backup failed, nothing else we can do until user fixes the issue manually.
        // There is no way to bring user attention in daemon mode so we just update status and
        // keep spamming if debug is on.
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::CheckAutomaticBackup -- ERROR! Failed to create automatic backup.\n");
        strAutoDenomResult = _("ERROR! Failed to create automatic backup") + ", " + _("see debug.log for details.");
        return false;
    case -2:
        // We were able to create automatic backup but keypool was not replenished because wallet is locked.
        // There is no way to bring user attention in daemon mode so we just update status and
        // keep spamming if debug is on.
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::CheckAutomaticBackup -- WARNING! Failed to create replenish keypool, please unlock your wallet to do so.\n");
        strAutoDenomResult = _("WARNING! Failed to replenish keypool, please unlock your wallet to do so.") + ", " + _("see debug.log for details.");
        return false;
    }

    if (GetWallets()[0]->nKeysLeftSinceAutoBackup < PRIVATESEND_KEYS_THRESHOLD_STOP) {
        // We should never get here via mixing itself but probably something else is still actively using keypool
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::CheckAutomaticBackup -- Very low number of keys left: %d, no mixing available.\n", GetWallets()[0]->nKeysLeftSinceAutoBackup);
        strAutoDenomResult = strprintf(_("Very low number of keys left: %d") + ", " + _("no mixing available."), GetWallets()[0]->nKeysLeftSinceAutoBackup);
        // It's getting really dangerous, stop mixing
        fPrivateSendRunning = false;
        return false;
    } else if (GetWallets()[0]->nKeysLeftSinceAutoBackup < PRIVATESEND_KEYS_THRESHOLD_WARNING) {
        // Low number of keys left but it's still more or less safe to continue
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::CheckAutomaticBackup -- Very low number of keys left: %d\n", GetWallets()[0]->nKeysLeftSinceAutoBackup);
        strAutoDenomResult = strprintf(_("Very low number of keys left: %d"), GetWallets()[0]->nKeysLeftSinceAutoBackup);

        if (fCreateAutoBackups) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::CheckAutomaticBackup -- Trying to create new backup.\n");
            std::string warningString;
            std::string errorString;

            if (!GetWallets()[0]->AutoBackupWallet("", warningString, errorString)) {
                if (!warningString.empty()) {
                    // There were some issues saving backup but yet more or less safe to continue
                    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::CheckAutomaticBackup -- WARNING! Something went wrong on automatic backup: %s\n", warningString);
                }
                if (!errorString.empty()) {
                    // Things are really broken
                    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::CheckAutomaticBackup -- ERROR! Failed to create automatic backup: %s\n", errorString);
                    strAutoDenomResult = strprintf(_("ERROR! Failed to create automatic backup") + ": %s", errorString);
                    return false;
                }
            }
        } else {
            // Wait for something else (e.g. GUI action) to create automatic backup for us
            return false;
        }
    }

    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::CheckAutomaticBackup -- Keys left since latest backup: %d\n", GetWallets()[0]->nKeysLeftSinceAutoBackup);

    return true;
}

//
// Passively run mixing in the background to mix funds based on the given configuration.
//
bool CPrivateSendClientSession::DoAutomaticDenominating(CConnman& connman, bool fDryRun)
{
    if (fMasternodeMode) return false; // no client-side mixing on masternodes
    if (nState != POOL_STATE_IDLE) return false;

    if (!masternodeSync.IsBlockchainSynced()) {
        strAutoDenomResult = _("Can't mix while sync in progress.");
        return false;
    }

    if (!privateSendClient.fEnablePrivateSend || !privateSendClient.fPrivateSendRunning) return false;

    CAmount nBalanceNeedsAnonymized;

    {
        LOCK2(cs_main, mempool.cs);
        LOCK(GetWallets()[0]->cs_wallet);

        if (!fDryRun && GetWallets()[0]->IsLocked(true)) {
            strAutoDenomResult = _("Wallet is locked.");
            return false;
        }

        if (GetEntriesCount() > 0) {
            strAutoDenomResult = _("Mixing in progress...");
            return false;
        }

        TRY_LOCK(cs_privatesend, lockDS);
        if (!lockDS) {
            strAutoDenomResult = _("Lock is already in place.");
            return false;
        }

        if (deterministicMNManager->GetListAtChainTip().GetValidMNsCount() == 0 &&
            Params().NetworkIDString() != CBaseChainParams::REGTEST) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::DoAutomaticDenominating -- No Masternodes detected\n");
            strAutoDenomResult = _("No Masternodes detected.");
            return false;
        }

        // check if there is anything left to do
        CAmount nBalanceAnonymized = GetWallets()[0]->GetAnonymizedBalance();
        nBalanceNeedsAnonymized = privateSendClient.nPrivateSendAmount*COIN - nBalanceAnonymized;

        if (nBalanceNeedsAnonymized < 0) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::DoAutomaticDenominating -- Nothing to do\n");
            // nothing to do, just keep it in idle mode
            return false;
        }

        CAmount nValueMin = CPrivateSend::GetSmallestDenomination();

        // if there are no confirmed DS collateral inputs yet
        if (!GetWallets()[0]->HasCollateralInputs()) {
            // should have some additional amount for them
            nValueMin += CPrivateSend::GetMaxCollateralAmount();
        }

        // including denoms but applying some restrictions
        CAmount nBalanceAnonymizable = GetWallets()[0]->GetAnonymizableBalance();

        // mixable balance is way too small
        if (nBalanceAnonymizable < nValueMin) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::DoAutomaticDenominating -- Not enough funds to mix\n");
            strAutoDenomResult = _("Not enough funds to mix.");
            return false;
        }

        // excluding denoms
        CAmount nBalanceAnonimizableNonDenom = GetWallets()[0]->GetAnonymizableBalance(true);
        // denoms
        CAmount nBalanceDenominatedConf = GetWallets()[0]->GetDenominatedBalance();
        CAmount nBalanceDenominatedUnconf = GetWallets()[0]->GetDenominatedBalance(true);
        CAmount nBalanceDenominated = nBalanceDenominatedConf + nBalanceDenominatedUnconf;
        CAmount nBalanceToDenominate = privateSendClient.nPrivateSendAmount * COIN - nBalanceDenominated;

        // adjust nBalanceNeedsAnonymized to consume final denom
        if (nBalanceDenominated - nBalanceAnonymized > nBalanceNeedsAnonymized) {
            auto denoms = CPrivateSend::GetStandardDenominations();
            CAmount nAdditionalDenom{0};
            for (const auto& denom : denoms) {
                if (nBalanceNeedsAnonymized < denom) {
                    nAdditionalDenom = denom;
                } else {
                    break;
                }
            }
            nBalanceNeedsAnonymized += nAdditionalDenom;
        }

        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::DoAutomaticDenominating -- current stats:\n"
            "    nValueMin: %s\n"
            "    nBalanceAnonymizable: %s\n"
            "    nBalanceAnonymized: %s\n"
            "    nBalanceNeedsAnonymized: %s\n"
            "    nBalanceAnonimizableNonDenom: %s\n"
            "    nBalanceDenominatedConf: %s\n"
            "    nBalanceDenominatedUnconf: %s\n"
            "    nBalanceDenominated: %s\n"
            "    nBalanceToDenominate: %s\n",
            FormatMoney(nValueMin),
            FormatMoney(nBalanceAnonymizable),
            FormatMoney(nBalanceAnonymized),
            FormatMoney(nBalanceNeedsAnonymized),
            FormatMoney(nBalanceAnonimizableNonDenom),
            FormatMoney(nBalanceDenominatedConf),
            FormatMoney(nBalanceDenominatedUnconf),
            FormatMoney(nBalanceDenominated),
            FormatMoney(nBalanceToDenominate)
            );

        if (fDryRun) return true;

        // Check if we have should create more denominated inputs i.e.
        // there are funds to denominate and denominated balance does not exceed
        // max amount to mix yet.
        if (nBalanceAnonimizableNonDenom >= nValueMin + CPrivateSend::GetCollateralAmount() && nBalanceToDenominate > 0) {
            CreateDenominated(nBalanceToDenominate, connman);
        }

        //check if we have the collateral sized inputs
        if (!GetWallets()[0]->HasCollateralInputs()) {
            return !GetWallets()[0]->HasCollateralInputs(false) && MakeCollateralAmounts(connman);
        }

        if (nSessionID) {
            strAutoDenomResult = _("Mixing in progress...");
            return false;
        }

        // Initial phase, find a Masternode
        // Clean if there is anything left from previous session
        UnlockCoins();
        keyHolderStorage.ReturnAll();
        SetNull();

        // should be no unconfirmed denoms in non-multi-session mode
        if (!privateSendClient.fPrivateSendMultiSession && nBalanceDenominatedUnconf > 0) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::DoAutomaticDenominating -- Found unconfirmed denominated outputs, will wait till they confirm to continue.\n");
            strAutoDenomResult = _("Found unconfirmed denominated outputs, will wait till they confirm to continue.");
            return false;
        }

        //check our collateral and create new if needed
        std::string strReason;
        if (txMyCollateral == CMutableTransaction()) {
            if (!GetWallets()[0]->CreateCollateralTransaction(txMyCollateral, strReason)) {
                LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::DoAutomaticDenominating -- create collateral error:%s\n", strReason);
                return false;
            }
        } else {
            if (!CPrivateSend::IsCollateralValid(txMyCollateral)) {
                LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::DoAutomaticDenominating -- invalid collateral, recreating...\n");
                if (!GetWallets()[0]->CreateCollateralTransaction(txMyCollateral, strReason)) {
                    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::DoAutomaticDenominating -- create collateral error: %s\n", strReason);
                    return false;
                }
            }
        }
        // lock the funds we're going to use for our collateral
        for (const auto& txin : txMyCollateral.vin) {
            GetWallets()[0]->LockCoin(txin.prevout);
            vecOutPointLocked.push_back(txin.prevout);
        }
    } // LOCK2(cs_main, GetWallets()[0]->cs_wallet);

    // Always attempt to join an existing queue
    if (JoinExistingQueue(nBalanceNeedsAnonymized, connman)) {
        return true;
    }

    // If we were unable to find/join an existing queue then start a new one.
    if (StartNewQueue(nBalanceNeedsAnonymized, connman)) return true;

    strAutoDenomResult = _("No compatible Masternode found.");
    return false;
}

bool CPrivateSendClientManager::DoAutomaticDenominating(CConnman& connman, bool fDryRun)
{
    if (fMasternodeMode) return false; // no client-side mixing on masternodes
    if (!fEnablePrivateSend || !fPrivateSendRunning) return false;

    if (!masternodeSync.IsBlockchainSynced()) {
        strAutoDenomResult = _("Can't mix while sync in progress.");
        return false;
    }

    if (!fDryRun && GetWallets()[0]->IsLocked(true)) {
        strAutoDenomResult = _("Wallet is locked.");
        return false;
    }

    int nMnCountEnabled = deterministicMNManager->GetListAtChainTip().GetValidMNsCount();

    // If we've used 90% of the Masternode list then drop the oldest first ~30%
    int nThreshold_high = nMnCountEnabled * 0.9;
    int nThreshold_low = nThreshold_high * 0.7;
    LogPrint(BCLog::PRIVATESEND, "Checking vecMasternodesUsed: size: %d, threshold: %d\n", (int)vecMasternodesUsed.size(), nThreshold_high);

    if ((int)vecMasternodesUsed.size() > nThreshold_high) {
        vecMasternodesUsed.erase(vecMasternodesUsed.begin(), vecMasternodesUsed.begin() + vecMasternodesUsed.size() - nThreshold_low);
        LogPrint(BCLog::PRIVATESEND, "  vecMasternodesUsed: new size: %d, threshold: %d\n", (int)vecMasternodesUsed.size(), nThreshold_high);
    }

    LOCK(cs_deqsessions);
    bool fResult = true;
    if ((int)deqSessions.size() < nPrivateSendSessions) {
        deqSessions.emplace_back();
    }
    for (auto& session : deqSessions) {
        if (!CheckAutomaticBackup()) return false;

        if (WaitForAnotherBlock()) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::DoAutomaticDenominating -- Last successful PrivateSend action was too recent\n");
            strAutoDenomResult = _("Last successful PrivateSend action was too recent.");
            return false;
        }

        fResult &= session.DoAutomaticDenominating(connman, fDryRun);
    }

    return fResult;
}

void CPrivateSendClientManager::AddUsedMasternode(const COutPoint& outpointMn)
{
    vecMasternodesUsed.push_back(outpointMn);
}

CDeterministicMNCPtr CPrivateSendClientManager::GetRandomNotUsedMasternode()
{
    auto mnList = deterministicMNManager->GetListAtChainTip();

    int nCountEnabled = mnList.GetValidMNsCount();
    int nCountNotExcluded = nCountEnabled - vecMasternodesUsed.size();

    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::%s -- %d enabled masternodes, %d masternodes to choose from\n", __func__, nCountEnabled, nCountNotExcluded);
    if(nCountNotExcluded < 1) {
        return nullptr;
    }

    // fill a vector
    std::vector<CDeterministicMNCPtr> vpMasternodesShuffled;
    vpMasternodesShuffled.reserve((size_t)nCountEnabled);
    mnList.ForEachMN(true, [&](const CDeterministicMNCPtr& dmn) {
        vpMasternodesShuffled.emplace_back(dmn);
    });

    FastRandomContext insecure_rand;
    // shuffle pointers
    std::random_shuffle(vpMasternodesShuffled.begin(), vpMasternodesShuffled.end(), insecure_rand);

    std::set<COutPoint> excludeSet(vecMasternodesUsed.begin(), vecMasternodesUsed.end());

    // loop through
    for (const auto& dmn : vpMasternodesShuffled) {
        if (excludeSet.count(dmn->collateralOutpoint)) {
            continue;
        }

        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::%s -- found, masternode=%s\n", __func__, dmn->collateralOutpoint.ToStringShort());
        return dmn;
    }

    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::%s -- failed\n", __func__);
    return nullptr;
}

bool CPrivateSendClientSession::JoinExistingQueue(CAmount nBalanceNeedsAnonymized, CConnman& connman)
{
    if (!privateSendClient.fEnablePrivateSend || !privateSendClient.fPrivateSendRunning) return false;

    auto mnList = deterministicMNManager->GetListAtChainTip();

    // Look through the queues and see if anything matches
    CPrivateSendQueue dsq;
    while (privateSendClient.GetQueueItemAndTry(dsq)) {
        auto dmn = mnList.GetValidMNByCollateral(dsq.masternodeOutpoint);

        if (!dmn) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::JoinExistingQueue -- dsq masternode is not in masternode list, masternode=%s\n", dsq.masternodeOutpoint.ToStringShort());
            continue;
        }

        // skip next mn payments winners
        if (dmn->pdmnState->nLastPaidHeight + mnList.GetValidMNsCount() < mnList.GetHeight() + 8) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::JoinExistingQueue -- skipping winner, masternode=%s\n", dmn->proTxHash.ToString());
            continue;
        }

        // mixing rate limit i.e. nLastDsq check should already pass in DSQUEUE ProcessMessage
        // in order for dsq to get into vecPrivateSendQueue, so we should be safe to mix already,
        // no need for additional verification here

        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::JoinExistingQueue -- trying queue: %s\n", dsq.ToString());

        std::vector<std::pair<CTxDSIn, CTxOut> > vecPSInOutPairsTmp;

        // Try to match their denominations if possible, select exact number of denominations
        if (!GetWallets()[0]->SelectPSInOutPairsByDenominations(dsq.nDenom, nBalanceNeedsAnonymized, vecPSInOutPairsTmp)) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::JoinExistingQueue -- Couldn't match denomination %d (%s)\n", dsq.nDenom, CPrivateSend::DenominationToString(dsq.nDenom));
            continue;
        }

        privateSendClient.AddUsedMasternode(dsq.masternodeOutpoint);

        if (connman.IsMasternodeOrDisconnectRequested(dmn->pdmnState->addr)) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::JoinExistingQueue -- skipping masternode connection, addr=%s\n", dmn->pdmnState->addr.ToString());
            continue;
        }

        nSessionDenom = dsq.nDenom;
        mixingMasternode = dmn;
        pendingDsaRequest = CPendingDsaRequest(dmn->pdmnState->addr, CPrivateSendAccept(nSessionDenom, txMyCollateral));
        connman.AddPendingMasternode(dmn->proTxHash);
        // TODO: add new state POOL_STATE_CONNECTING and bump MIN_PRIVATESEND_PEER_PROTO_VERSION
        SetState(POOL_STATE_QUEUE);
        nTimeLastSuccessfulStep = GetTime();
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::JoinExistingQueue -- pending connection (from queue): nSessionDenom: %d (%s), addr=%s\n",
            nSessionDenom, CPrivateSend::DenominationToString(nSessionDenom), dmn->pdmnState->addr.ToString());
        strAutoDenomResult = _("Trying to connect...");
        return true;
    }
    strAutoDenomResult = _("Failed to find mixing queue to join");
    return false;
}

bool CPrivateSendClientSession::StartNewQueue(CAmount nBalanceNeedsAnonymized, CConnman& connman)
{
    if (!privateSendClient.fEnablePrivateSend || !privateSendClient.fPrivateSendRunning) return false;
    if (nBalanceNeedsAnonymized <= 0) return false;

    int nTries = 0;
    auto mnList = deterministicMNManager->GetListAtChainTip();
    int nMnCount = mnList.GetValidMNsCount();

    // find available denominated amounts
    std::set<CAmount> setAmounts;
    if (!GetWallets()[0]->SelectDenominatedAmounts(nBalanceNeedsAnonymized, setAmounts)) {
        // this should never happen
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::StartNewQueue -- Can't mix: no compatible inputs found!\n");
        strAutoDenomResult = _("Can't mix: no compatible inputs found!");
        return false;
    }

    // otherwise, try one randomly
    while (nTries < 10) {
        auto dmn = privateSendClient.GetRandomNotUsedMasternode();

        if (!dmn) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::StartNewQueue -- Can't find random masternode!\n");
            strAutoDenomResult = _("Can't find random Masternode.");
            return false;
        }

        privateSendClient.AddUsedMasternode(dmn->collateralOutpoint);

        // skip next mn payments winners
        if (dmn->pdmnState->nLastPaidHeight + nMnCount < mnList.GetHeight() + 8) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::StartNewQueue -- skipping winner, masternode=%s\n", dmn->proTxHash.ToString());
            nTries++;
            continue;
        }

        int64_t nLastDsq = mmetaman.GetMetaInfo(dmn->proTxHash)->GetLastDsq();
        int64_t nDsqThreshold = mmetaman.GetDsqThreshold(dmn->proTxHash, nMnCount);
        if (nLastDsq != 0 && nDsqThreshold > mmetaman.GetDsqCount()) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::StartNewQueue -- Too early to mix on this masternode!"
                      " masternode=%s  addr=%s  nLastDsq=%d  nDsqThreshold=%d  nDsqCount=%d\n",
                dmn->proTxHash.ToString(), dmn->pdmnState->addr.ToString(), nLastDsq,
                nDsqThreshold, mmetaman.GetDsqCount());
            nTries++;
            continue;
        }

        if (connman.IsMasternodeOrDisconnectRequested(dmn->pdmnState->addr)) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::StartNewQueue -- skipping masternode connection, addr=%s\n", dmn->pdmnState->addr.ToString());
            nTries++;
            continue;
        }

        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::StartNewQueue -- attempt %d connection to Masternode %s\n", nTries, dmn->pdmnState->addr.ToString());

        // try to get a single random denom out of setAmounts
        while (nSessionDenom == 0) {
            for (auto it = setAmounts.rbegin(); it != setAmounts.rend(); ++it) {
                if (setAmounts.size() > 1 && GetRandInt(2)) continue;
                nSessionDenom = CPrivateSend::AmountToDenomination(*it);
                break;
            }
        }

        mixingMasternode = dmn;
        connman.AddPendingMasternode(dmn->proTxHash);
        pendingDsaRequest = CPendingDsaRequest(dmn->pdmnState->addr, CPrivateSendAccept(nSessionDenom, txMyCollateral));
        // TODO: add new state POOL_STATE_CONNECTING and bump MIN_PRIVATESEND_PEER_PROTO_VERSION
        SetState(POOL_STATE_QUEUE);
        nTimeLastSuccessfulStep = GetTime();
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::StartNewQueue -- pending connection, nSessionDenom: %d (%s), addr=%s\n",
            nSessionDenom, CPrivateSend::DenominationToString(nSessionDenom), dmn->pdmnState->addr.ToString());
        strAutoDenomResult = _("Trying to connect...");
        return true;
    }
    strAutoDenomResult = _("Failed to start a new mixing queue");
    return false;
}

bool CPrivateSendClientSession::ProcessPendingDsaRequest(CConnman& connman)
{
    if (!pendingDsaRequest) return false;

    bool fDone = connman.ForNode(pendingDsaRequest.GetAddr(), [&](CNode* pnode) {
        LogPrint(BCLog::PRIVATESEND, "-- processing dsa queue for addr=%s\n", pnode->addr.ToString());
        nTimeLastSuccessfulStep = GetTime();
        // TODO: this vvvv should be here after new state POOL_STATE_CONNECTING is added and MIN_PRIVATESEND_PEER_PROTO_VERSION is bumped
        // SetState(POOL_STATE_QUEUE);
        CNetMsgMaker msgMaker(pnode->GetSendVersion());
        connman.PushMessage(pnode, msgMaker.Make(NetMsgType::DSACCEPT, pendingDsaRequest.GetDSA()));
        return true;
    });

    if (fDone) {
        pendingDsaRequest = CPendingDsaRequest();
    } else if (pendingDsaRequest.IsExpired()) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::%s -- failed to connect to %s\n", __func__, pendingDsaRequest.GetAddr().ToString());
        SetNull();
    }

    return fDone;
}

void CPrivateSendClientManager::ProcessPendingDsaRequest(CConnman& connman)
{
    LOCK(cs_deqsessions);
    for (auto& session : deqSessions) {
        if (session.ProcessPendingDsaRequest(connman)) {
            strAutoDenomResult = _("Mixing in progress...");
        }
    }
}

bool CPrivateSendClientSession::SubmitDenominate(CConnman& connman)
{
    LOCK2(cs_main, mempool.cs);
    LOCK(GetWallets()[0]->cs_wallet);

    std::string strError;
    std::vector<std::pair<CTxDSIn, CTxOut> > vecPSInOutPairs, vecPSInOutPairsTmp;

    if (!SelectDenominate(strError, vecPSInOutPairs)) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::SubmitDenominate -- SelectDenominate failed, error: %s\n", strError);
        return false;
    }

    std::vector<std::pair<int, size_t> > vecInputsByRounds;

    for (int i = 0; i < privateSendClient.nPrivateSendRounds; i++) {
        if (PrepareDenominate(i, i, strError, vecPSInOutPairs, vecPSInOutPairsTmp, true)) {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::SubmitDenominate -- Running PrivateSend denominate for %d rounds, success\n", i);
            vecInputsByRounds.emplace_back(i, vecPSInOutPairsTmp.size());
        } else {
            LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::SubmitDenominate -- Running PrivateSend denominate for %d rounds, error: %s\n", i, strError);
        }
    }

    // more inputs first, for equal input count prefer the one with less rounds
    std::sort(vecInputsByRounds.begin(), vecInputsByRounds.end(), [](const auto& a, const auto& b) {
        return a.second > b.second || (a.second == b.second && a.first < b.first);
    });

    LogPrint(BCLog::PRIVATESEND, "vecInputsByRounds for denom %d\n", nSessionDenom);
    for (const auto& pair : vecInputsByRounds) {
        LogPrint(BCLog::PRIVATESEND, "vecInputsByRounds: rounds: %d, inputs: %d\n", pair.first, pair.second);
    }

    int nRounds = vecInputsByRounds.begin()->first;
    if (PrepareDenominate(nRounds, nRounds, strError, vecPSInOutPairs, vecPSInOutPairsTmp)) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::SubmitDenominate -- Running PrivateSend denominate for %d rounds, success\n", nRounds);
        return SendDenominate(vecPSInOutPairsTmp, connman);
    }

    // We failed? That's strange but let's just make final attempt and try to mix everything
    if (PrepareDenominate(0, privateSendClient.nPrivateSendRounds - 1, strError, vecPSInOutPairs, vecPSInOutPairsTmp)) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::SubmitDenominate -- Running PrivateSend denominate for all rounds, success\n");
        return SendDenominate(vecPSInOutPairsTmp, connman);
    }

    // Should never actually get here but just in case
    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::SubmitDenominate -- Running PrivateSend denominate for all rounds, error: %s\n", strError);
    strAutoDenomResult = strError;
    return false;
}

bool CPrivateSendClientSession::SelectDenominate(std::string& strErrorRet, std::vector<std::pair<CTxDSIn, CTxOut> >& vecPSInOutPairsRet)
{
    if (!privateSendClient.fEnablePrivateSend || !privateSendClient.fPrivateSendRunning) return false;

    if (GetWallets()[0]->IsLocked(true)) {
        strErrorRet = "Wallet locked, unable to create transaction!";
        return false;
    }

    if (GetEntriesCount() > 0) {
        strErrorRet = "Already have pending entries in the PrivateSend pool";
        return false;
    }

    vecPSInOutPairsRet.clear();

    bool fSelected = GetWallets()[0]->SelectPSInOutPairsByDenominations(nSessionDenom, CPrivateSend::GetMaxPoolAmount(), vecPSInOutPairsRet);
    if (!fSelected) {
        strErrorRet = "Can't select current denominated inputs";
        return false;
    }

    return true;
}

bool CPrivateSendClientSession::PrepareDenominate(int nMinRounds, int nMaxRounds, std::string& strErrorRet, const std::vector<std::pair<CTxDSIn, CTxOut> >& vecPSInOutPairsIn, std::vector<std::pair<CTxDSIn, CTxOut> >& vecPSInOutPairsRet, bool fDryRun)
{
    AssertLockHeld(cs_main);
    AssertLockHeld(GetWallets()[0]->cs_wallet);

    if (!CPrivateSend::IsValidDenomination(nSessionDenom)) {
        strErrorRet = "Incorrect session denom";
        return false;
    }
    CAmount nDenomAmount = CPrivateSend::DenominationToAmount(nSessionDenom);

    // NOTE: No need to randomize order of inputs because they were
    // initially shuffled in CWallet::SelectPSInOutPairsByDenominations already.
    int nSteps{0};
    vecPSInOutPairsRet.clear();

    // Try to add up to PRIVATESEND_ENTRY_MAX_SIZE of every needed denomination
    for (const auto& pair : vecPSInOutPairsIn) {
        if (nSteps >= PRIVATESEND_ENTRY_MAX_SIZE) break;
        if (pair.second.nRounds < nMinRounds || pair.second.nRounds > nMaxRounds) continue;
        if (pair.second.nValue != nDenomAmount) continue;

        CScript scriptDenom;
        if (fDryRun) {
            scriptDenom = CScript();
        } else {
            // randomly skip some inputs when we have at least one of the same denom already
            // TODO: make it adjustable via options/cmd-line params
            if (nSteps >= 1 && GetRandInt(5) == 0) {
                // still count it as a step to randomize number of inputs
                // if we have more than (or exactly) PRIVATESEND_ENTRY_MAX_SIZE of them
                ++nSteps;
                continue;
            }
            scriptDenom = keyHolderStorage.AddKey(GetWallets()[0]);
        }
        vecPSInOutPairsRet.emplace_back(pair.first, CTxOut(nDenomAmount, scriptDenom));
        // step is complete
        ++nSteps;
    }

    if (vecPSInOutPairsRet.empty()) {
        keyHolderStorage.ReturnAll();
        strErrorRet = "Can't prepare current denominated outputs";
        return false;
    }

    if (fDryRun) {
        return true;
    }

    for (const auto& pair : vecPSInOutPairsRet) {
        GetWallets()[0]->LockCoin(pair.first.prevout);
        vecOutPointLocked.push_back(pair.first.prevout);
    }

    return true;
}

// Create collaterals by looping through inputs grouped by addresses
bool CPrivateSendClientSession::MakeCollateralAmounts(CConnman& connman)
{
    if (!privateSendClient.fEnablePrivateSend || !privateSendClient.fPrivateSendRunning) return false;

    LOCK2(cs_main, mempool.cs);
    LOCK(GetWallets()[0]->cs_wallet);

    // NOTE: We do not allow txes larger than 100kB, so we have to limit number of inputs here.
    // We still want to consume a lot of inputs to avoid creating only smaller denoms though.
    // Knowing that each CTxIn is at least 148b big, 400 inputs should take 400 x ~148b = ~60kB.
    // This still leaves more than enough room for another data of typical MakeCollateralAmounts tx.
    std::vector<CompactTallyItem> vecTally;
    if (!GetWallets()[0]->SelectCoinsGroupedByAddresses(vecTally, false, false, true, 400)) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::MakeCollateralAmounts -- SelectCoinsGroupedByAddresses can't find any inputs!\n");
        return false;
    }

    // Start from smallest balances first to consume tiny amounts and cleanup UTXO a bit
    std::sort(vecTally.begin(), vecTally.end(), [](const CompactTallyItem& a, const CompactTallyItem& b) {
        return a.nAmount < b.nAmount;
    });

    // First try to use only non-denominated funds
    for (const auto& item : vecTally) {
        if (!MakeCollateralAmounts(item, false, connman)) continue;
        return true;
    }

    // There should be at least some denominated funds we should be able to break in pieces to continue mixing
    for (const auto& item : vecTally) {
        if (!MakeCollateralAmounts(item, true, connman)) continue;
        return true;
    }

    // If we got here then something is terribly broken actually
    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::MakeCollateralAmounts -- ERROR: Can't make collaterals!\n");
    return false;
}

// Split up large inputs or create fee sized inputs
bool CPrivateSendClientSession::MakeCollateralAmounts(const CompactTallyItem& tallyItem, bool fTryDenominated, CConnman& connman)
{
    AssertLockHeld(cs_main);
    AssertLockHeld(mempool.cs);
    AssertLockHeld(GetWallets()[0]->cs_wallet);

    if (!privateSendClient.fEnablePrivateSend || !privateSendClient.fPrivateSendRunning) return false;

    // Skip way too tiny amounts
    if (tallyItem.nAmount < CPrivateSend::GetCollateralAmount()) {
        return false;
    }

    // Skip single inputs that can be used as collaterals already
    if (tallyItem.vecOutPoints.size() == 1 && CPrivateSend::IsCollateralAmount(tallyItem.nAmount)) {
        return false;
    }

    // denominated input is always a single one, so we can check its amount directly and return early
    if (!fTryDenominated && tallyItem.vecOutPoints.size() == 1 && CPrivateSend::IsDenominatedAmount(tallyItem.nAmount)) {
        return false;
    }

    CWalletTx wtx;
    CAmount nFeeRet = 0;
    int nChangePosRet = -1;
    std::string strFail = "";
    std::vector<CRecipient> vecSend;

    // make our collateral address
    CReserveKey reservekeyCollateral(GetWallets()[0]);
    // make our change address
    CReserveKey reservekeyChange(GetWallets()[0]);

    CScript scriptCollateral;
    CPubKey vchPubKey;
    assert(reservekeyCollateral.GetReservedKey(vchPubKey, false)); // should never fail, as we just unlocked
    scriptCollateral = GetScriptForDestination(vchPubKey.GetID());

    CAmount nCollateralAmount{0};
    if (tallyItem.nAmount > CPrivateSend::GetMaxCollateralAmount() + CPrivateSend::GetCollateralAmount()*2) {
        // Change output will be large enough to be valid as a collateral or a source input for another run
        nCollateralAmount = CPrivateSend::GetMaxCollateralAmount();
    } else {
        // Change output might be too small for another collateral if we try to create the largest collateral
        // here, create a slightly smaller one instead
        nCollateralAmount = CPrivateSend::GetMaxCollateralAmount() - CPrivateSend::GetCollateralAmount();
    }
    vecSend.push_back((CRecipient){scriptCollateral, nCollateralAmount, false});

    // try to use non-denominated and not mn-like funds first, select them explicitly
    CCoinControl coinControl;
    coinControl.fAllowOtherInputs = false;
    coinControl.fAllowWatchOnly = false;
    coinControl.nCoinType = CoinType::ONLY_NONDENOMINATED;
    // send change to the same address so that we were able create more denoms out of it later
    coinControl.destChange = tallyItem.txdest;
    for (const auto& outpoint : tallyItem.vecOutPoints) {
        coinControl.Select(outpoint);
    }

    bool fSuccess = GetWallets()[0]->CreateTransaction(vecSend, wtx, reservekeyChange,
        nFeeRet, nChangePosRet, strFail, coinControl);
    if (!fSuccess) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::MakeCollateralAmounts -- ONLY_NONDENOMINATED: %s\n", strFail);
        // If we failed then most likely there are not enough funds on this address.
        if (fTryDenominated) {
            // Try to also use denominated coins (we can't mix denominated without collaterals anyway).
            coinControl.nCoinType = CoinType::ALL_COINS;
            if (!GetWallets()[0]->CreateTransaction(vecSend, wtx, reservekeyChange,
                    nFeeRet, nChangePosRet, strFail, coinControl)) {
                LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::MakeCollateralAmounts -- ALL_COINS Error: %s\n", strFail);
                reservekeyCollateral.ReturnKey();
                return false;
            }
        } else {
            // Nothing else we can do.
            reservekeyCollateral.ReturnKey();
            return false;
        }
    }

    reservekeyCollateral.KeepKey();

    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::MakeCollateralAmounts -- txid=%s\n", wtx.GetHash().GetHex());

    // use the same nCachedLastSuccessBlock as for DS mixing to prevent race
    CValidationState state;
    if (!GetWallets()[0]->CommitTransaction(wtx, reservekeyChange, &connman, state)) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::MakeCollateralAmounts -- CommitTransaction failed! Reason given: %s\n", state.GetRejectReason());
        return false;
    }

    privateSendClient.UpdatedSuccessBlock();

    return true;
}

// Create denominations by looping through inputs grouped by addresses
bool CPrivateSendClientSession::CreateDenominated(CAmount nBalanceToDenominate, CConnman& connman)
{
    if (!privateSendClient.fEnablePrivateSend || !privateSendClient.fPrivateSendRunning) return false;

    LOCK2(cs_main, mempool.cs);
    LOCK(GetWallets()[0]->cs_wallet);

    // NOTE: We do not allow txes larger than 100kB, so we have to limit number of inputs here.
    // We still want to consume a lot of inputs to avoid creating only smaller denoms though.
    // Knowing that each CTxIn is at least 148b big, 400 inputs should take 400 x ~148b = ~60kB.
    // This still leaves more than enough room for another data of typical CreateDenominated tx.
    std::vector<CompactTallyItem> vecTally;
    if (!GetWallets()[0]->SelectCoinsGroupedByAddresses(vecTally, true, true, true, 400)) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::CreateDenominated -- SelectCoinsGroupedByAddresses can't find any inputs!\n");
        return false;
    }

    // Start from largest balances first to speed things up by creating txes with larger/largest denoms included
    std::sort(vecTally.begin(), vecTally.end(), [](const CompactTallyItem& a, const CompactTallyItem& b) {
        return a.nAmount > b.nAmount;
    });

    bool fCreateMixingCollaterals = !GetWallets()[0]->HasCollateralInputs();

    for (const auto& item : vecTally) {
        if (!CreateDenominated(nBalanceToDenominate, item, fCreateMixingCollaterals, connman)) continue;
        return true;
    }

    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::CreateDenominated -- failed!\n");
    return false;
}

// Create denominations
bool CPrivateSendClientSession::CreateDenominated(CAmount nBalanceToDenominate, const CompactTallyItem& tallyItem, bool fCreateMixingCollaterals, CConnman& connman)
{
    AssertLockHeld(cs_main);
    AssertLockHeld(mempool.cs);
    AssertLockHeld(GetWallets()[0]->cs_wallet);

    if (!privateSendClient.fEnablePrivateSend || !privateSendClient.fPrivateSendRunning) return false;

    std::vector<CRecipient> vecSend;
    CKeyHolderStorage keyHolderStorageDenom;

    CAmount nValueLeft = tallyItem.nAmount;
    nValueLeft -= CPrivateSend::GetCollateralAmount(); // leave some room for fees

    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::CreateDenominated -- 0 - %s nValueLeft: %f\n", EncodeDestination(tallyItem.txdest), (float)nValueLeft / COIN);

    // ****** Add an output for mixing collaterals ************ /

    if (fCreateMixingCollaterals) {
        CScript scriptCollateral = keyHolderStorageDenom.AddKey(GetWallets()[0]);
        vecSend.push_back((CRecipient){scriptCollateral, CPrivateSend::GetMaxCollateralAmount(), false});
        nValueLeft -= CPrivateSend::GetMaxCollateralAmount();
    }

    // ****** Add outputs for denoms ************ /

    int nOutputsTotal = 0;
    bool fAddFinal = true;
    std::vector<CAmount> vecStandardDenoms = CPrivateSend::GetStandardDenominations();

    std::map<CAmount, int> mapDenomCount;
    for (auto nDenomValue : vecStandardDenoms) {
        mapDenomCount.insert(std::pair<CAmount, int>(nDenomValue, GetWallets()[0]->CountInputsWithAmount(nDenomValue)));
    }

    int debugLoopNum = 0;

    // NOTE: We do not allow txes larger than 100kB, so we have to limit the number of outputs here.
    // We still want to create a lot of outputs though.
    // Knowing that each CTxOut is ~35b big, 400 inputs should take 400 x ~35b = ~17.5kb.
    // More than 500 outputs starts to make qt quite laggy.
    // Additionally to need all 500 outputs (assuming a max per denom of 50) you'd need to be trying to
    // create denominations for over 3000 dash!

    // Will generate outputs for the createdenoms up to privatesendmaxdenoms per denom

    // This works in the way creating PS denoms has traditionally worked, assuming enough funds,
    // it will start with the smallest denom then create 11 of those, then go up to the next biggest denom create 11
    // and repeat. Previously, once the largest denom was reached, as many would be created were created as possible and
    // then any remaining was put into a change address and denominations were created in the same manner a block later.
    // Now, in this system, so long as we don't reach 500 outputs the process repeats in the same transaction,
    // creating up to privatesenddenoms per denomination in a single transaction.
    while (nValueLeft >= CPrivateSend::GetSmallestDenomination() && nOutputsTotal <= 500) {
        assert(debugLoopNum++ < 100);
        LogPrint(BCLog::PRIVATESEND,
                 "CPrivateSendClientSession::CreateDenominated -- big loop %d\n",
                 debugLoopNum);

        for (auto it = vecStandardDenoms.rbegin(); it != vecStandardDenoms.rend(); ++it) {
            CAmount nDenomValue = *it;

            int nOutputs = 0;

            auto needMoreOutputs = [&]() {
                bool fRegular = (nValueLeft >= nDenomValue && nBalanceToDenominate >= nDenomValue);
                bool fFinal = (fAddFinal
                               && nValueLeft >= nDenomValue
                               && nBalanceToDenominate > 0
                               && nBalanceToDenominate < nDenomValue);
                fAddFinal = false; // add final denom only once, only the smalest possible one

                if (fRegular == false && fFinal == false) {
                    LogPrint(BCLog::PRIVATESEND,
                             "CPrivateSendClientSession::CreateDenominated -- fRegular false b/c (nValueLeft %d >= nDenomValue %d && nBalanceToDenominate %d >= nDenomValue %d)\n",
                             nValueLeft, nDenomValue, nBalanceToDenominate, nDenomValue);
                    LogPrint(BCLog::PRIVATESEND,
                             "CPrivateSendClientSession::CreateDenominated -- fFinal false b/c %s  nValueLeft: %d nDenomValue: %d nBalanceToD: %d\n",
                             fAddFinal ? "true" : "false", nValueLeft, nDenomValue, nBalanceToDenominate);
                }

                return fRegular || fFinal;
            };

            // add each output up to 11 times or until it can't be added again or until we reach nPrivateSendDenoms
            while (needMoreOutputs() && nOutputs <= 10 && mapDenomCount.find(nDenomValue)->second <= privateSendClient.nPrivateSendDenoms) {
                LogPrint(BCLog::PRIVATESEND,
                         "CPrivateSendClientSession::CreateDenominated -- CreatedDenom b/c mapDenomCount.find(nDenomValue)->second: %d <= privateSendClient.nPrivateSendDenoms: %d\n",
                         mapDenomCount.find(nDenomValue)->second, privateSendClient.nPrivateSendDenoms);
                CScript scriptDenom = keyHolderStorageDenom.AddKey(GetWallets()[0]);

                vecSend.push_back((CRecipient) {scriptDenom, nDenomValue, false});

                // increment outputs and subtract denomination amount
                nOutputs++;
                mapDenomCount.find(nDenomValue)->second++;
                nValueLeft -= nDenomValue;
                nBalanceToDenominate -= nDenomValue;
                LogPrint(BCLog::PRIVATESEND,
                         "CPrivateSendClientSession::CreateDenominated -- 1 - totalOutputs: %d, nOutputsTotal: %d, nOutputs: %d, nValueLeft: %f\n",
                         nOutputsTotal + nOutputs, nOutputsTotal, nOutputs, (float) nValueLeft / COIN);
            }
/*
            LogPrint(BCLog::PRIVATESEND,
                     "CPrivateSendClientSession::CreateDenominated -- Stopped CreatedDenom for %d args: %d %s %d %d eval: %s\n",
                     nDenomValue, needMoreOutputs() ? "true" : "false", nOutputs, mapDenomCount.find(nDenomValue)->second, privateSendClient.nPrivateSendDenoms,
                     (needMoreOutputs() && nOutputs <= 10 && mapDenomCount.find(nDenomValue)->second <= privateSendClient.nPrivateSendDenoms) ? "true" : "false");
*/
            nOutputsTotal += nOutputs;
            if (nValueLeft == 0 || nBalanceToDenominate <= 0) {
                LogPrint(BCLog::PRIVATESEND,
                         "CPrivateSendClientSession::CreateDenominated -- THIS WEIRD BREAK WAS CALLED %d, %d\n",
                         nValueLeft, nBalanceToDenominate);

                break;
            }
        }

        bool finished = true;
        for (auto it : mapDenomCount) {
            // Check if this specific denom could use another loop, check that there aren't nPrivateSendDenoms of this
            // denom and that our nValueLeft/nBalanceToDenominate is enough to create one of these denoms, if so, loop again.
            if (it.second <= privateSendClient.nPrivateSendDenoms && nValueLeft >= it.first && nBalanceToDenominate >= it.first) {
                LogPrint(BCLog::PRIVATESEND,
                         "CPrivateSendClientSession::CreateDenominated -- 1 - decided not finished b/c %d <= %d && %d >= %d\n",
                         it.second, privateSendClient.nPrivateSendDenoms, nValueLeft, it.first);
                finished = false;
                break;
            }
            LogPrint(BCLog::PRIVATESEND,
                     "CPrivateSendClientSession::CreateDenominated -- 1 - Okay to finish b/c %d <= %d && %d >= %d\n",
                     it.second, privateSendClient.nPrivateSendDenoms, nValueLeft, it.first);

        }

        if (finished) break;
    }

    // Now that nPrivateSendDenoms worth of each denom have been created, do something with the remainder.
    while (nValueLeft >= CPrivateSend::GetSmallestDenomination() && nOutputsTotal <= 500) {

        // Go big to small
        for (long nDenomValue : vecStandardDenoms) {
            int nOutputs = 0;

            // Number of denoms we can create given our denom and the amount of funds we have left
            int denomsToCreate = nValueLeft / nDenomValue;
            for (int i = 0; i < denomsToCreate; i++) {
                CScript scriptDenom = keyHolderStorageDenom.AddKey(GetWallets()[0]);

                vecSend.push_back((CRecipient) {scriptDenom, nDenomValue, false});

                // increment outputs and subtract denomination amount
                nOutputs++;
                mapDenomCount.find(nDenomValue)->second++;
                nValueLeft -= nDenomValue;
                nBalanceToDenominate -= nDenomValue;
                LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::CreateDenominated -- Incremented a denom %d. currentDenomCount: %d\n",
                         mapDenomCount.find(nDenomValue)->first, mapDenomCount.find(nDenomValue)->second);
                LogPrint(BCLog::PRIVATESEND,
                         "CPrivateSendClientSession::CreateDenominated -- 1 - totalOutputs: %d, nOutputsTotal: %d, nOutputs: %d, nValueLeft: %f\n",
                         nOutputsTotal + nOutputs, nOutputsTotal, nOutputs, (float) nValueLeft / COIN);

            }
            nOutputsTotal += nOutputs;
        }
    }

    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::CreateDenominated -- 2 - nOutputsTotal: %d, nValueLeft: %f\n", nOutputsTotal, (float)nValueLeft / COIN);

    // No reasons to create mixing collaterals if we can't create denoms to mix
    if (nOutputsTotal == 0) return false;

    // if we have anything left over, it will be automatically send back as change - there is no need to send it manually

    CCoinControl coinControl;
    coinControl.fAllowOtherInputs = false;
    coinControl.fAllowWatchOnly = false;
    coinControl.nCoinType = CoinType::ONLY_NONDENOMINATED;
    // send change to the same address so that we were able create more denoms out of it later
    coinControl.destChange = tallyItem.txdest;
    for (const auto& outpoint : tallyItem.vecOutPoints) {
        coinControl.Select(outpoint);
    }

    CWalletTx wtx;
    CAmount nFeeRet = 0;
    int nChangePosRet = -1;
    std::string strFail = "";
    // make our change address
    CReserveKey reservekeyChange(GetWallets()[0]);

    bool fSuccess = GetWallets()[0]->CreateTransaction(vecSend, wtx, reservekeyChange,
        nFeeRet, nChangePosRet, strFail, coinControl);
    if (!fSuccess) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::CreateDenominated -- Error: %s\n", strFail);
        keyHolderStorageDenom.ReturnAll();
        return false;
    }

    keyHolderStorageDenom.KeepAll();

    CValidationState state;
    if (!GetWallets()[0]->CommitTransaction(wtx, reservekeyChange, &connman, state)) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::CreateDenominated -- CommitTransaction failed! Reason given: %s\n", state.GetRejectReason());
        return false;
    }

    // use the same nCachedLastSuccessBlock as for DS mixing to prevent race
    privateSendClient.UpdatedSuccessBlock();
    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::CreateDenominated -- txid=%s\n", wtx.GetHash().GetHex());

    return true;
}

void CPrivateSendClientSession::RelayIn(const CPrivateSendEntry& entry, CConnman& connman)
{
    if (!mixingMasternode) return;

    connman.ForNode(mixingMasternode->pdmnState->addr, [&entry, &connman](CNode* pnode) {
        LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::RelayIn -- found master, relaying message to %s\n", pnode->addr.ToString());
        CNetMsgMaker msgMaker(pnode->GetSendVersion());
        connman.PushMessage(pnode, msgMaker.Make(NetMsgType::DSVIN, entry));
        return true;
    });
}

void CPrivateSendClientSession::SetState(PoolState nStateNew)
{
    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientSession::SetState -- nState: %d, nStateNew: %d\n", nState, nStateNew);
    nState = nStateNew;
}

void CPrivateSendClientManager::UpdatedBlockTip(const CBlockIndex* pindex)
{
    nCachedBlockHeight = pindex->nHeight;
    LogPrint(BCLog::PRIVATESEND, "CPrivateSendClientManager::UpdatedBlockTip -- nCachedBlockHeight: %d\n", nCachedBlockHeight);
}

void CPrivateSendClientManager::DoMaintenance(CConnman& connman)
{
    if (!fEnablePrivateSend) return;
    if (fMasternodeMode) return; // no client-side mixing on masternodes

    if (!masternodeSync.IsBlockchainSynced() || ShutdownRequested()) return;

    static unsigned int nTick = 0;
    static unsigned int nDoAutoNextRun = nTick + PRIVATESEND_AUTO_TIMEOUT_MIN;

    nTick++;
    CheckTimeout();
    ProcessPendingDsaRequest(connman);
    if (nDoAutoNextRun == nTick) {
        DoAutomaticDenominating(connman);
        nDoAutoNextRun = nTick + PRIVATESEND_AUTO_TIMEOUT_MIN + GetRandInt(PRIVATESEND_AUTO_TIMEOUT_MAX - PRIVATESEND_AUTO_TIMEOUT_MIN);
    }
}

void CPrivateSendClientSession::GetJsonInfo(UniValue& obj) const
{
    obj.clear();
    obj.setObject();
    if (mixingMasternode != nullptr) {
        assert(mixingMasternode->pdmnState);
        obj.push_back(Pair("protxhash", mixingMasternode->proTxHash.ToString()));
        obj.push_back(Pair("outpoint",  mixingMasternode->collateralOutpoint.ToStringShort()));
        obj.push_back(Pair("service",   mixingMasternode->pdmnState->addr.ToString()));
    }
    obj.push_back(Pair("denomination",  ValueFromAmount(CPrivateSend::DenominationToAmount(nSessionDenom))));
    obj.push_back(Pair("state",         GetStateString()));
    obj.push_back(Pair("entries_count", GetEntriesCount()));
}

void CPrivateSendClientManager::GetJsonInfo(UniValue& obj) const
{
    LOCK(cs_deqsessions);
    obj.clear();
    obj.setObject();
    obj.push_back(Pair("enabled",       fEnablePrivateSend));
    obj.push_back(Pair("running",       fPrivateSendRunning));
    obj.push_back(Pair("multisession",  fPrivateSendMultiSession));
    obj.push_back(Pair("max_sessions",  nPrivateSendSessions));
    obj.push_back(Pair("max_rounds",    nPrivateSendRounds));
    obj.push_back(Pair("max_amount",    nPrivateSendAmount));
    obj.push_back(Pair("max_denoms",    nPrivateSendDenoms));
    obj.push_back(Pair("queue_size",    GetQueueSize()));

    UniValue arrSessions(UniValue::VARR);
    for (const auto& session : deqSessions) {
        if (session.GetState() != POOL_STATE_IDLE) {
            UniValue objSession(UniValue::VOBJ);
            session.GetJsonInfo(objSession);
            arrSessions.push_back(objSession);
        }
    }
    obj.push_back(Pair("sessions",  arrSessions));
}
