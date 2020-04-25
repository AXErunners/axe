// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/init.h>

#include <keepass.h>
#include <net.h>
#include <util.h>
#include <utilmoneystr.h>
#include <validation.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>
#include <wallet/walletutil.h>

#include <privatesend/privatesend-client.h>

std::string WalletInit::GetHelpString(bool showDebug)
{
    std::string strUsage = HelpMessageGroup(_("Wallet options:"));
    strUsage += HelpMessageOpt("-disablewallet", _("Do not load the wallet and disable wallet RPC calls"));
    strUsage += HelpMessageOpt("-keypool=<n>", strprintf(_("Set key pool size to <n> (default: %u)"), DEFAULT_KEYPOOL_SIZE));
    strUsage += HelpMessageOpt("-fallbackfee=<amt>", strprintf(_("A fee rate (in %s/kB) that will be used when fee estimation has insufficient data (default: %s)"),
                                                               CURRENCY_UNIT, FormatMoney(DEFAULT_FALLBACK_FEE)));
    strUsage += HelpMessageOpt("-discardfee=<amt>", strprintf(_("The fee rate (in %s/kB) that indicates your tolerance for discarding change by adding it to the fee (default: %s). "
                                                                "Note: An output is discarded if it is dust at this rate, but we will always discard up to the dust relay fee and a discard fee above that is limited by the fee estimate for the longest target"),
                                                              CURRENCY_UNIT, FormatMoney(DEFAULT_DISCARD_FEE)));
    strUsage += HelpMessageOpt("-mintxfee=<amt>", strprintf(_("Fees (in %s/kB) smaller than this are considered zero fee for transaction creation (default: %s)"),
                                                            CURRENCY_UNIT, FormatMoney(DEFAULT_TRANSACTION_MINFEE)));
    strUsage += HelpMessageOpt("-paytxfee=<amt>", strprintf(_("Fee (in %s/kB) to add to transactions you send (default: %s)"),
                                                            CURRENCY_UNIT, FormatMoney(payTxFee.GetFeePerK())));
    strUsage += HelpMessageOpt("-rescan", _("Rescan the block chain for missing wallet transactions on startup"));
    strUsage += HelpMessageOpt("-salvagewallet", _("Attempt to recover private keys from a corrupt wallet on startup"));
    strUsage += HelpMessageOpt("-spendzeroconfchange", strprintf(_("Spend unconfirmed change when sending transactions (default: %u)"), DEFAULT_SPEND_ZEROCONF_CHANGE));
    strUsage += HelpMessageOpt("-txconfirmtarget=<n>", strprintf(_("If paytxfee is not set, include enough fee so transactions begin confirmation on average within n blocks (default: %u)"), DEFAULT_TX_CONFIRM_TARGET));
    strUsage += HelpMessageOpt("-usehd", _("Use hierarchical deterministic key generation (HD) after BIP39/BIP44. Only has effect during wallet creation/first start") + " " + strprintf(_("(default: %u)"), DEFAULT_USE_HD_WALLET));
    strUsage += HelpMessageOpt("-mnemonic=<text>", _("User defined mnemonic for HD wallet (bip39). Only has effect during wallet creation/first start (default: randomly generated)"));
    strUsage += HelpMessageOpt("-mnemonicpassphrase=<text>", _("User defined mnemonic passphrase for HD wallet (BIP39). Only has effect during wallet creation/first start (default: empty string)"));
    strUsage += HelpMessageOpt("-hdseed=<hex>", _("User defined seed for HD wallet (should be in hex). Only has effect during wallet creation/first start (default: randomly generated)"));
    strUsage += HelpMessageOpt("-upgradewallet", _("Upgrade wallet to latest format on startup"));
    strUsage += HelpMessageOpt("-wallet=<file>", _("Specify wallet file (within data directory)") + " " + strprintf(_("(default: %s)"), DEFAULT_WALLET_DAT));
    strUsage += HelpMessageOpt("-walletbroadcast", _("Make the wallet broadcast transactions") + " " + strprintf(_("(default: %u)"), DEFAULT_WALLETBROADCAST));
    strUsage += HelpMessageOpt("-walletdir=<dir>", _("Specify directory to hold wallets (default: <datadir>/wallets if it exists, otherwise <datadir>)"));
    strUsage += HelpMessageOpt("-walletnotify=<cmd>", _("Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)"));
    strUsage += HelpMessageOpt("-zapwallettxes=<mode>", _("Delete all wallet transactions and only recover those parts of the blockchain through -rescan on startup") +
                                                        " " + _("(1 = keep tx meta data e.g. account owner and payment request information, 2 = drop tx meta data)"));
    strUsage += HelpMessageOpt("-createwalletbackups=<n>", strprintf(_("Number of automatic wallet backups (default: %u)"), nWalletBackups));
    strUsage += HelpMessageOpt("-walletbackupsdir=<dir>", _("Specify full path to directory for automatic wallet backups (must exist)"));
    strUsage += HelpMessageOpt("-keepass", strprintf(_("Use KeePass 2 integration using KeePassHttp plugin (default: %u)"), 0));
    strUsage += HelpMessageOpt("-keepassport=<port>", strprintf(_("Connect to KeePassHttp on port <port> (default: %u)"), DEFAULT_KEEPASS_HTTP_PORT));
    strUsage += HelpMessageOpt("-keepasskey=<key>", _("KeePassHttp key for AES encrypted communication with KeePass"));
    strUsage += HelpMessageOpt("-keepassid=<id>", _("KeePassHttp id for the established association"));
    strUsage += HelpMessageOpt("-keepassname=<name>", _("Name to construct url for KeePass entry that stores the wallet passphrase"));

    strUsage += HelpMessageGroup(_("PrivateSend options:"));
    strUsage += HelpMessageOpt("-enableprivatesend", strprintf(_("Enable use of PrivateSend for funds stored in this wallet (0-1, default: %u)"), 0));
    strUsage += HelpMessageOpt("-privatesendautostart", strprintf(_("Start PrivateSend automatically (0-1, default: %u)"), DEFAULT_PRIVATESEND_AUTOSTART));
    strUsage += HelpMessageOpt("-privatesendmultisession", strprintf(_("Enable multiple PrivateSend mixing sessions per block, experimental (0-1, default: %u)"), DEFAULT_PRIVATESEND_MULTISESSION));
    strUsage += HelpMessageOpt("-privatesendsessions=<n>", strprintf(_("Use N separate masternodes in parallel to mix funds (%u-%u, default: %u)"), MIN_PRIVATESEND_SESSIONS, MAX_PRIVATESEND_SESSIONS, DEFAULT_PRIVATESEND_SESSIONS));
    strUsage += HelpMessageOpt("-privatesendrounds=<n>", strprintf(_("Use N separate masternodes for each denominated input to mix funds (%u-%u, default: %u)"), MIN_PRIVATESEND_ROUNDS, MAX_PRIVATESEND_ROUNDS, DEFAULT_PRIVATESEND_ROUNDS));
    strUsage += HelpMessageOpt("-privatesendamount=<n>", strprintf(_("Target PrivateSend balance (%u-%u, default: %u)"), MIN_PRIVATESEND_AMOUNT, MAX_PRIVATESEND_AMOUNT, DEFAULT_PRIVATESEND_AMOUNT));
    strUsage += HelpMessageOpt("-privatesenddenoms=<n>", strprintf(_("Create up to N inputs of each denominated amount (%u-%u, default: %u)"), MIN_PRIVATESEND_DENOMS, MAX_PRIVATESEND_DENOMS, DEFAULT_PRIVATESEND_DENOMS));

    if (showDebug)
    {
        strUsage += HelpMessageGroup(_("Wallet debugging/testing options:"));

        strUsage += HelpMessageOpt("-dblogsize=<n>", strprintf("Flush wallet database activity from memory to disk log every <n> megabytes (default: %u)", DEFAULT_WALLET_DBLOGSIZE));
        strUsage += HelpMessageOpt("-flushwallet", strprintf("Run a thread to flush wallet periodically (default: %u)", DEFAULT_FLUSHWALLET));
        strUsage += HelpMessageOpt("-privdb", strprintf("Sets the DB_PRIVATE flag in the wallet db environment (default: %u)", DEFAULT_WALLET_PRIVDB));
        strUsage += HelpMessageOpt("-walletrejectlongchains", strprintf(_("Wallet will not create transactions that violate mempool chain limits (default: %u)"), DEFAULT_WALLET_REJECT_LONG_CHAINS));
    }

    return strUsage;
}

bool WalletInit::ParameterInteraction()
{
    if (gArgs.IsArgSet("-masternodeblsprivkey") && gArgs.SoftSetBoolArg("-disablewallet", true)) {
        LogPrintf("%s: parameter interaction: -masternodeblsprivkey set -> setting -disablewallet=1\n", __func__);
    }

    if (gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
        for (const std::string& wallet : gArgs.GetArgs("-wallet")) {
            LogPrintf("%s: parameter interaction: -disablewallet -> ignoring -wallet=%s\n", __func__, wallet);
        }

        return true;
    } else if (gArgs.IsArgSet("-masternodeblsprivkey")) {
        return InitError(_("You can not start a masternode with wallet enabled."));
    }

    gArgs.SoftSetArg("-wallet", DEFAULT_WALLET_DAT);
    const bool is_multiwallet = gArgs.GetArgs("-wallet").size() > 1;

    if (gArgs.GetBoolArg("-blocksonly", DEFAULT_BLOCKSONLY) && gArgs.SoftSetBoolArg("-walletbroadcast", false)) {
        LogPrintf("%s: parameter interaction: -blocksonly=1 -> setting -walletbroadcast=0\n", __func__);
    }

    if (gArgs.GetBoolArg("-salvagewallet", false)) {
        if (is_multiwallet) {
            return InitError(strprintf("%s is only allowed with a single wallet file", "-salvagewallet"));
        }
        // Rewrite just private keys: rescan to find transactions
        if (gArgs.SoftSetBoolArg("-rescan", true)) {
            LogPrintf("%s: parameter interaction: -salvagewallet=1 -> setting -rescan=1\n", __func__);
        }
    }

    int zapwallettxes = gArgs.GetArg("-zapwallettxes", 0);
    // -zapwallettxes implies dropping the mempool on startup
    if (zapwallettxes != 0 && gArgs.SoftSetBoolArg("-persistmempool", false)) {
        LogPrintf("%s: parameter interaction: -zapwallettxes=%s -> setting -persistmempool=0\n", __func__, zapwallettxes);
    }

    // -zapwallettxes implies a rescan
    if (zapwallettxes != 0) {
        if (is_multiwallet) {
            return InitError(strprintf("%s is only allowed with a single wallet file", "-zapwallettxes"));
        }
        if (gArgs.SoftSetBoolArg("-rescan", true)) {
            LogPrintf("%s: parameter interaction: -zapwallettxes=%s -> setting -rescan=1\n", __func__, zapwallettxes);
        }
    }

    if (is_multiwallet) {
        if (gArgs.GetBoolArg("-upgradewallet", false)) {
            return InitError(strprintf("%s is only allowed with a single wallet file", "-upgradewallet"));
        }
    }

    if (gArgs.GetBoolArg("-sysperms", false))
        return InitError("-sysperms is not allowed in combination with enabled wallet functionality");
    if (gArgs.GetArg("-prune", 0) && gArgs.GetBoolArg("-rescan", false))
        return InitError(_("Rescans are not possible in pruned mode. You will need to use -reindex which will download the whole blockchain again."));

    if (::minRelayTxFee.GetFeePerK() > HIGH_TX_FEE_PER_KB)
        InitWarning(AmountHighWarn("-minrelaytxfee") + " " +
                    _("The wallet will avoid paying less than the minimum relay fee."));

    if (gArgs.IsArgSet("-mintxfee"))
    {
        CAmount n = 0;
        if (!ParseMoney(gArgs.GetArg("-mintxfee", ""), n) || 0 == n)
            return InitError(AmountErrMsg("mintxfee", gArgs.GetArg("-mintxfee", "")));
        if (n > HIGH_TX_FEE_PER_KB)
            InitWarning(AmountHighWarn("-mintxfee") + " " +
                        _("This is the minimum transaction fee you pay on every transaction."));
        CWallet::minTxFee = CFeeRate(n);
    }
    if (gArgs.IsArgSet("-fallbackfee"))
    {
        CAmount nFeePerK = 0;
        if (!ParseMoney(gArgs.GetArg("-fallbackfee", ""), nFeePerK))
            return InitError(strprintf(_("Invalid amount for -fallbackfee=<amount>: '%s'"), gArgs.GetArg("-fallbackfee", "")));
        if (nFeePerK > HIGH_TX_FEE_PER_KB)
            InitWarning(AmountHighWarn("-fallbackfee") + " " +
                        _("This is the transaction fee you may pay when fee estimates are not available."));
        CWallet::fallbackFee = CFeeRate(nFeePerK);
    }
    if (gArgs.IsArgSet("-discardfee"))
    {
        CAmount nFeePerK = 0;
        if (!ParseMoney(gArgs.GetArg("-discardfee", ""), nFeePerK))
            return InitError(strprintf(_("Invalid amount for -discardfee=<amount>: '%s'"), gArgs.GetArg("-discardfee", "")));
        if (nFeePerK > HIGH_TX_FEE_PER_KB)
            InitWarning(AmountHighWarn("-discardfee") + " " +
                        _("This is the transaction fee you may discard if change is smaller than dust at this level"));
        CWallet::m_discard_rate = CFeeRate(nFeePerK);
    }
    if (gArgs.IsArgSet("-paytxfee"))
    {
        CAmount nFeePerK = 0;
        if (!ParseMoney(gArgs.GetArg("-paytxfee", ""), nFeePerK))
            return InitError(AmountErrMsg("paytxfee", gArgs.GetArg("-paytxfee", "")));
        if (nFeePerK > HIGH_TX_FEE_PER_KB)
            InitWarning(AmountHighWarn("-paytxfee") + " " +
                        _("This is the transaction fee you will pay if you send a transaction."));

        payTxFee = CFeeRate(nFeePerK, 1000);
        if (payTxFee < ::minRelayTxFee)
        {
            return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s' (must be at least %s)"),
                                       gArgs.GetArg("-paytxfee", ""), ::minRelayTxFee.ToString()));
        }
    }
    if (gArgs.IsArgSet("-maxtxfee"))
    {
        CAmount nMaxFee = 0;
        if (!ParseMoney(gArgs.GetArg("-maxtxfee", ""), nMaxFee))
            return InitError(AmountErrMsg("maxtxfee", gArgs.GetArg("-maxtxfee", "")));
        if (nMaxFee > HIGH_MAX_TX_FEE)
            InitWarning(_("-maxtxfee is set very high! Fees this large could be paid on a single transaction."));
        maxTxFee = nMaxFee;
        if (CFeeRate(maxTxFee, 1000) < ::minRelayTxFee)
        {
            return InitError(strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s' (must be at least the minrelay fee of %s to prevent stuck transactions)"),
                                       gArgs.GetArg("-maxtxfee", ""), ::minRelayTxFee.ToString()));
        }
    }
    nTxConfirmTarget = gArgs.GetArg("-txconfirmtarget", DEFAULT_TX_CONFIRM_TARGET);
    bSpendZeroConfChange = gArgs.GetBoolArg("-spendzeroconfchange", DEFAULT_SPEND_ZEROCONF_CHANGE);

    if (gArgs.IsArgSet("-walletbackupsdir")) {
        if (!fs::is_directory(gArgs.GetArg("-walletbackupsdir", ""))) {
            LogPrintf("%s: Warning: incorrect parameter -walletbackupsdir, path must exist! Using default path.\n", __func__);
            InitWarning("Warning: incorrect parameter -walletbackupsdir, path must exist! Using default path.\n");

            gArgs.ForceRemoveArg("-walletbackupsdir");
        }
    }

    if (gArgs.IsArgSet("-hdseed") && IsHex(gArgs.GetArg("-hdseed", "not hex")) && (gArgs.IsArgSet("-mnemonic") || gArgs.IsArgSet("-mnemonicpassphrase"))) {
        gArgs.ForceRemoveArg("-mnemonic");
        gArgs.ForceRemoveArg("-mnemonicpassphrase");
        LogPrintf("%s: parameter interaction: can't use -hdseed and -mnemonic/-mnemonicpassphrase together, will prefer -seed\n", __func__);
    }

    return true;
}

void WalletInit::RegisterRPC(CRPCTable &t)
{
    if (gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
        return;
    }

    RegisterWalletRPCCommands(t);
}

bool WalletInit::Verify()
{
    if (gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
        return true;
    }

    if (gArgs.IsArgSet("-walletdir")) {
        fs::path wallet_dir = gArgs.GetArg("-walletdir", "");
        if (!fs::exists(wallet_dir)) {
            return InitError(strprintf(_("Specified -walletdir \"%s\" does not exist"), wallet_dir.string()));
        } else if (!fs::is_directory(wallet_dir)) {
            return InitError(strprintf(_("Specified -walletdir \"%s\" is not a directory"), wallet_dir.string()));
        } else if (!wallet_dir.is_absolute()) {
            return InitError(strprintf(_("Specified -walletdir \"%s\" is a relative path"), wallet_dir.string()));
        }
    }

    LogPrintf("Using wallet directory %s\n", GetWalletDir().string());

    uiInterface.InitMessage(_("Verifying wallet(s)..."));

    // Keep track of each wallet absolute path to detect duplicates.
    std::set<fs::path> wallet_paths;

    for (const std::string& walletFile : gArgs.GetArgs("-wallet")) {
        if (boost::filesystem::path(walletFile).filename() != walletFile) {
            return InitError(strprintf(_("Error loading wallet %s. -wallet parameter must only specify a filename (not a path)."), walletFile));
        }

        if (SanitizeString(walletFile, SAFE_CHARS_FILENAME) != walletFile) {
            return InitError(strprintf(_("Error loading wallet %s. Invalid characters in -wallet filename."), walletFile));
        }

        fs::path wallet_path = fs::absolute(walletFile, GetWalletDir());

        if (fs::exists(wallet_path) && (!fs::is_regular_file(wallet_path) || fs::is_symlink(wallet_path))) {
            return InitError(strprintf(_("Error loading wallet %s. -wallet filename must be a regular file."), walletFile));
        }

        if (!wallet_paths.insert(wallet_path).second) {
            return InitError(strprintf(_("Error loading wallet %s. Duplicate -wallet filename specified."), walletFile));
        }

        std::string strError;
        if (!CWalletDB::VerifyEnvironment(walletFile, GetWalletDir().string(), strError)) {
            return InitError(strError);
        }

        if (gArgs.GetBoolArg("-salvagewallet", false)) {
            // Recover readable keypairs:
            CWallet dummyWallet;
            std::string backup_filename;
            if (!CWalletDB::Recover(walletFile, (void *)&dummyWallet, CWalletDB::RecoverKeysOnlyFilter, backup_filename)) {
                return false;
            }
        }

        std::string strWarning;
        bool dbV = CWalletDB::VerifyDatabaseFile(walletFile, GetWalletDir().string(), strWarning, strError);
        if (!strWarning.empty()) {
            InitWarning(strWarning);
        }
        if (!dbV) {
            InitError(strError);
            return false;
        }
    }

    return true;
}

bool WalletInit::Open()
{
    if (gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
        LogPrintf("Wallet disabled!\n");
        return true;
    }

    for (const std::string& walletFile : gArgs.GetArgs("-wallet")) {
        CWallet * const pwallet = CWallet::CreateWalletFromFile(walletFile);
        if (!pwallet) {
            return false;
        }
        vpwallets.push_back(pwallet);
    }

    return true;
}

void WalletInit::Start(CScheduler& scheduler)
{
    for (CWalletRef pwallet : vpwallets) {
        pwallet->postInitProcess(scheduler);
    }
    // Only do this once
    vpwallets[0]->schedulePrivateSendClientMaintenance(scheduler);

}

void WalletInit::Flush()
{
    if (privateSendClient.fEnablePrivateSend) {
        // Stop PrivateSend, release keys
        privateSendClient.fPrivateSendRunning = false;
        privateSendClient.ResetPool();
    }
    for (CWalletRef pwallet : vpwallets) {
        pwallet->Flush(false);
    }
}

void WalletInit::Stop()
{
    for (CWalletRef pwallet : vpwallets) {
        pwallet->Flush(true);
    }
}

void WalletInit::Close()
{
    for (CWalletRef pwallet : vpwallets) {
        delete pwallet;
    }
    vpwallets.clear();
}

void WalletInit::AutoLockMasternodeCollaterals()
{
    // we can't do this before DIP3 is fully initialized
    for (CWalletRef pwallet : vpwallets) {
        pwallet->AutoLockMasternodeCollaterals();
    }
}

void WalletInit::InitPrivateSendSettings()
{
    if (vpwallets.empty()) {
        privateSendClient.fEnablePrivateSend = privateSendClient.fPrivateSendRunning = false;
    } else {
        privateSendClient.fEnablePrivateSend = gArgs.GetBoolArg("-enableprivatesend", !fLiteMode);
        privateSendClient.fPrivateSendRunning = vpwallets[0]->IsLocked() ? false : gArgs.GetBoolArg("-privatesendautostart", DEFAULT_PRIVATESEND_AUTOSTART);
    }
    privateSendClient.fPrivateSendMultiSession = gArgs.GetBoolArg("-privatesendmultisession", DEFAULT_PRIVATESEND_MULTISESSION);
    privateSendClient.nPrivateSendSessions = std::min(std::max((int)gArgs.GetArg("-privatesendsessions", DEFAULT_PRIVATESEND_SESSIONS), MIN_PRIVATESEND_SESSIONS), MAX_PRIVATESEND_SESSIONS);
    privateSendClient.nPrivateSendRounds = std::min(std::max((int)gArgs.GetArg("-privatesendrounds", DEFAULT_PRIVATESEND_ROUNDS), MIN_PRIVATESEND_ROUNDS), MAX_PRIVATESEND_ROUNDS);
    privateSendClient.nPrivateSendAmount = std::min(std::max((int)gArgs.GetArg("-privatesendamount", DEFAULT_PRIVATESEND_AMOUNT), MIN_PRIVATESEND_AMOUNT), MAX_PRIVATESEND_AMOUNT);
    privateSendClient.nPrivateSendDenoms = std::min(std::max((int)gArgs.GetArg("-privatesenddenoms", DEFAULT_PRIVATESEND_DENOMS), MIN_PRIVATESEND_DENOMS), MAX_PRIVATESEND_DENOMS);

    if (privateSendClient.fEnablePrivateSend) {
        LogPrintf("PrivateSend: autostart=%d, multisession=%d, "
                  "sessions=%d, rounds=%d, amount=%d, denoms=%d\n",
                  privateSendClient.fPrivateSendRunning, privateSendClient.fPrivateSendMultiSession,
                  privateSendClient.nPrivateSendSessions, privateSendClient.nPrivateSendRounds,
                  privateSendClient.nPrivateSendAmount, privateSendClient.nPrivateSendDenoms);
    }

}

