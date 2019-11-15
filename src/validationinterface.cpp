// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "validationinterface.h"

static CMainSignals g_signals;

CMainSignals& GetMainSignals()
{
    return g_signals;
}

void RegisterValidationInterface(CValidationInterface* pwalletIn) {
    g_signals.AcceptedBlockHeader.connect(boost::bind(&CValidationInterface::AcceptedBlockHeader, pwalletIn, _1));
    g_signals.NotifyHeaderTip.connect(boost::bind(&CValidationInterface::NotifyHeaderTip, pwalletIn, _1, _2));
    g_signals.UpdatedBlockTip.connect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, _1, _2, _3));
    g_signals.TransactionAddedToMempool.connect(boost::bind(&CValidationInterface::TransactionAddedToMempool, pwalletIn, _1));
    g_signals.BlockConnected.connect(boost::bind(&CValidationInterface::BlockConnected, pwalletIn, _1, _2, _3));
    g_signals.BlockDisconnected.connect(boost::bind(&CValidationInterface::BlockDisconnected, pwalletIn, _1, _2));
    g_signals.NotifyTransactionLock.connect(boost::bind(&CValidationInterface::NotifyTransactionLock, pwalletIn, _1, _2));
    g_signals.NotifyChainLock.connect(boost::bind(&CValidationInterface::NotifyChainLock, pwalletIn, _1, _2));
    g_signals.SetBestChain.connect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, _1));
    g_signals.Inventory.connect(boost::bind(&CValidationInterface::Inventory, pwalletIn, _1));
    g_signals.Broadcast.connect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, _1, _2));
    g_signals.BlockChecked.connect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
    g_signals.ScriptForMining.connect(boost::bind(&CValidationInterface::GetScriptForMining, pwalletIn, _1));
    g_signals.NewPoWValidBlock.connect(boost::bind(&CValidationInterface::NewPoWValidBlock, pwalletIn, _1, _2));
    g_signals.NotifyGovernanceObject.connect(boost::bind(&CValidationInterface::NotifyGovernanceObject, pwalletIn, _1));
    g_signals.NotifyGovernanceVote.connect(boost::bind(&CValidationInterface::NotifyGovernanceVote, pwalletIn, _1));
    g_signals.NotifyInstantSendDoubleSpendAttempt.connect(boost::bind(&CValidationInterface::NotifyInstantSendDoubleSpendAttempt, pwalletIn, _1, _2));
    g_signals.NotifyMasternodeListChanged.connect(boost::bind(&CValidationInterface::NotifyMasternodeListChanged, pwalletIn, _1, _2, _3));
}

void UnregisterValidationInterface(CValidationInterface* pwalletIn) {
    g_signals.ScriptForMining.disconnect(boost::bind(&CValidationInterface::GetScriptForMining, pwalletIn, _1));
    g_signals.BlockChecked.disconnect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
    g_signals.Broadcast.disconnect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, _1, _2));
    g_signals.Inventory.disconnect(boost::bind(&CValidationInterface::Inventory, pwalletIn, _1));
    g_signals.SetBestChain.disconnect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, _1));
    g_signals.NotifyChainLock.disconnect(boost::bind(&CValidationInterface::NotifyChainLock, pwalletIn, _1, _2));
    g_signals.NotifyTransactionLock.disconnect(boost::bind(&CValidationInterface::NotifyTransactionLock, pwalletIn, _1, _2));
    g_signals.TransactionAddedToMempool.disconnect(boost::bind(&CValidationInterface::TransactionAddedToMempool, pwalletIn, _1));
    g_signals.BlockConnected.disconnect(boost::bind(&CValidationInterface::BlockConnected, pwalletIn, _1, _2, _3));
    g_signals.BlockDisconnected.disconnect(boost::bind(&CValidationInterface::BlockDisconnected, pwalletIn, _1, _2));
    g_signals.UpdatedBlockTip.disconnect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, _1, _2, _3));
    g_signals.NewPoWValidBlock.disconnect(boost::bind(&CValidationInterface::NewPoWValidBlock, pwalletIn, _1, _2));
    g_signals.NotifyHeaderTip.disconnect(boost::bind(&CValidationInterface::NotifyHeaderTip, pwalletIn, _1, _2));
    g_signals.AcceptedBlockHeader.disconnect(boost::bind(&CValidationInterface::AcceptedBlockHeader, pwalletIn, _1));
    g_signals.NotifyGovernanceObject.disconnect(boost::bind(&CValidationInterface::NotifyGovernanceObject, pwalletIn, _1));
    g_signals.NotifyGovernanceVote.disconnect(boost::bind(&CValidationInterface::NotifyGovernanceVote, pwalletIn, _1));
    g_signals.NotifyInstantSendDoubleSpendAttempt.disconnect(boost::bind(&CValidationInterface::NotifyInstantSendDoubleSpendAttempt, pwalletIn, _1, _2));
    g_signals.NotifyMasternodeListChanged.disconnect(boost::bind(&CValidationInterface::NotifyMasternodeListChanged, pwalletIn, _1, _2, _3));
}

void UnregisterAllValidationInterfaces() {
    g_signals.ScriptForMining.disconnect_all_slots();
    g_signals.BlockChecked.disconnect_all_slots();
    g_signals.Broadcast.disconnect_all_slots();
    g_signals.Inventory.disconnect_all_slots();
    g_signals.SetBestChain.disconnect_all_slots();
    g_signals.NotifyTransactionLock.disconnect_all_slots();
    g_signals.NotifyChainLock.disconnect_all_slots();
    g_signals.TransactionAddedToMempool.disconnect_all_slots();
    g_signals.BlockConnected.disconnect_all_slots();
    g_signals.BlockDisconnected.disconnect_all_slots();
    g_signals.UpdatedBlockTip.disconnect_all_slots();
    g_signals.NewPoWValidBlock.disconnect_all_slots();
    g_signals.NotifyHeaderTip.disconnect_all_slots();
    g_signals.AcceptedBlockHeader.disconnect_all_slots();
    g_signals.NotifyGovernanceObject.disconnect_all_slots();
    g_signals.NotifyGovernanceVote.disconnect_all_slots();
    g_signals.NotifyInstantSendDoubleSpendAttempt.disconnect_all_slots();
    g_signals.NotifyMasternodeListChanged.disconnect_all_slots();
}
