// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_INIT_H
#define BITCOIN_INIT_H

#include "wallet.h"

extern CWallet* pwalletMain;
void StartShutdown();
void Shutdown(void* parg);
bool AppInit2();

/*The help message mode determines what help message to show */
 enum HelpMessageMode
 {
    HMM_BITCOIND,
    HMM_BITCOIN_QT
 };

 std::string HelpMessage(HelpMessageMode mode);

#endif
