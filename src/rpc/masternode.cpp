// Copyright (c) 2014-2019 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "masternode/activemasternode.h"
#include "base58.h"
#include "clientversion.h"
#include "init.h"
#include "netbase.h"
#include "validation.h"
#include "util.h"
#include "utilmoneystr.h"
#include "txmempool.h"

#include "evo/specialtx.h"
#include "evo/deterministicmns.h"

#include "masternode/masternode-payments.h"
#include "masternode/masternode-sync.h"

#include "rpc/server.h"

#include "wallet/coincontrol.h"
#include "wallet/rpcwallet.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif // ENABLE_WALLET

#include <fstream>
#include <iomanip>
#include <univalue.h>

UniValue masternodelist(const JSONRPCRequest& request);

void masternode_list_help()
{
    throw std::runtime_error(
            "masternodelist ( \"mode\" \"filter\" )\n"
            "Get a list of masternodes in different modes. This call is identical to 'masternode list' call.\n"
            "\nArguments:\n"
            "1. \"mode\"      (string, optional/required to use filter, defaults = json) The mode to run list in\n"
            "2. \"filter\"    (string, optional) Filter results. Partial match by outpoint by default in all modes,\n"
            "                                    additional matches in some modes are also available\n"
            "\nAvailable modes:\n"
            "  addr           - Print ip address associated with a masternode (can be additionally filtered, partial match)\n"
            "  full           - Print info in format 'status payee lastpaidtime lastpaidblock IP'\n"
            "                   (can be additionally filtered, partial match)\n"
            "  info           - Print info in format 'status payee IP'\n"
            "                   (can be additionally filtered, partial match)\n"
            "  json           - Print info in JSON format (can be additionally filtered, partial match)\n"
            "  lastpaidblock  - Print the last block height a node was paid on the network\n"
            "  lastpaidtime   - Print the last time a node was paid on the network\n"
            "  owneraddress   - Print the masternode owner Axe address\n"
            "  payee          - Print the masternode payout Axe address (can be additionally filtered,\n"
            "                   partial match)\n"
            "  pubKeyOperator - Print the masternode operator public key\n"
            "  status         - Print masternode status: ENABLED / POSE_BANNED\n"
            "                   (can be additionally filtered, partial match)\n"
            "  votingaddress  - Print the masternode voting Axe address\n"
        );
}

UniValue masternode_list(const JSONRPCRequest& request)
{
    if (request.fHelp)
        masternode_list_help();
    JSONRPCRequest newRequest = request;
    newRequest.params.setArray();
    // forward params but skip "list"
    for (unsigned int i = 1; i < request.params.size(); i++) {
        newRequest.params.push_back(request.params[i]);
    }
    return masternodelist(newRequest);
}

void masternode_connect_help()
{
    throw std::runtime_error(
            "masternode connect \"address\"\n"
            "Connect to given masternode\n"
            "\nArguments:\n"
            "1. \"address\"      (string, required) The address of the masternode to connect\n"
        );
}

UniValue masternode_connect(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2)
        masternode_connect_help();

    std::string strAddress = request.params[1].get_str();

    CService addr;
    if (!Lookup(strAddress.c_str(), addr, 0, false))
        throw JSONRPCError(RPC_INTERNAL_ERROR, strprintf("Incorrect masternode address %s", strAddress));

    // TODO: Pass CConnman instance somehow and don't use global variable.
    g_connman->OpenMasternodeConnection(CAddress(addr, NODE_NETWORK));
    if (!g_connman->IsConnected(CAddress(addr, NODE_NETWORK), CConnman::AllNodes))
        throw JSONRPCError(RPC_INTERNAL_ERROR, strprintf("Couldn't connect to masternode %s", strAddress));

    return "successfully connected";
}

void masternode_count_help()
{
    throw std::runtime_error(
            "masternode count (\"mode\")\n"
            "  Get information about number of masternodes. Mode\n"
            "  usage is depricated, call without mode params returns\n"
            "  all values in JSON format.\n"
            "\nArguments:\n"
            "1. \"mode\"      (string, optional, DEPRICATED) Option to get number of masternodes in different states\n"
            "\nAvailable modes:\n"
            "  total         - total number of masternodes"
            "  ps            - number of PrivateSend compatible masternodes"
            "  enabled       - number of enabled masternodes"
            "  qualify       - number of qualified masternodes"
            "  all           - all above in one string"
        );
}

UniValue masternode_count(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
        masternode_count_help();

    auto mnList = deterministicMNManager->GetListAtChainTip();
    int total = mnList.GetAllMNsCount();
    int enabled = mnList.GetValidMNsCount();

    if (request.params.size() == 1) {
        UniValue obj(UniValue::VOBJ);

        obj.push_back(Pair("total", total));
        obj.push_back(Pair("enabled", enabled));

        return obj;
    }

    std::string strMode = request.params[1].get_str();

    if (strMode == "total")
        return total;

    if (strMode == "enabled")
        return enabled;

    if (strMode == "all")
        return strprintf("Total: %d (Enabled: %d)",
            total, enabled);

    throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown mode value");
}

UniValue GetNextMasternodeForPayment(int heightShift)
{
    auto mnList = deterministicMNManager->GetListAtChainTip();
    auto payees = mnList.GetProjectedMNPayees(heightShift);
    if (payees.empty())
        return "unknown";
    auto payee = payees.back();
    CScript payeeScript = payee->pdmnState->scriptPayout;

    CTxDestination payeeDest;
    ExtractDestination(payeeScript, payeeDest);

    UniValue obj(UniValue::VOBJ);

    obj.push_back(Pair("height",        mnList.GetHeight() + heightShift));
    obj.push_back(Pair("IP:port",       payee->pdmnState->addr.ToString()));
    obj.push_back(Pair("proTxHash",     payee->proTxHash.ToString()));
    obj.push_back(Pair("outpoint",      payee->collateralOutpoint.ToStringShort()));
    obj.push_back(Pair("payee",         IsValidDestination(payeeDest) ? EncodeDestination(payeeDest) : "UNKNOWN"));
    return obj;
}

void masternode_winner_help()
{
    throw std::runtime_error(
            "masternode winner\n"
            "Print info on next masternode winner to vote for\n"
        );
}

UniValue masternode_winner(const JSONRPCRequest& request)
{
    if (request.fHelp)
        masternode_winner_help();

    return GetNextMasternodeForPayment(10);
}

void masternode_current_help()
{
    throw std::runtime_error(
            "masternode current\n"
            "Print info on current masternode winner to be paid the next block (calculated locally)\n"
        );
}

UniValue masternode_current(const JSONRPCRequest& request)
{
    if (request.fHelp)
        masternode_current_help();

    return GetNextMasternodeForPayment(1);
}

#ifdef ENABLE_WALLET
void masternode_outputs_help()
{
    throw std::runtime_error(
            "masternode outputs\n"
            "Print masternode compatible outputs\n"
        );
}

UniValue masternode_outputs(const JSONRPCRequest& request)
{
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (request.fHelp)
        masternode_outputs_help();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    // Find possible candidates
    std::vector<COutput> vPossibleCoins;
    CCoinControl coin_control;
    coin_control.nCoinType = CoinType::ONLY_1000;
    pwallet->AvailableCoins(vPossibleCoins, true, &coin_control);

    UniValue obj(UniValue::VOBJ);
    for (const auto& out : vPossibleCoins) {
        obj.push_back(Pair(out.tx->GetHash().ToString(), strprintf("%d", out.i)));
    }

    return obj;
}

#endif // ENABLE_WALLET

void masternode_status_help()
{
    throw std::runtime_error(
            "masternode status\n"
            "Print masternode status information\n"
        );
}

UniValue masternode_status(const JSONRPCRequest& request)
{
    if (request.fHelp)
        masternode_status_help();

    if (!fMasternodeMode)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "This is not a masternode");

    UniValue mnObj(UniValue::VOBJ);

    // keep compatibility with legacy status for now (might get deprecated/removed later)
    mnObj.push_back(Pair("outpoint", activeMasternodeInfo.outpoint.ToStringShort()));
    mnObj.push_back(Pair("service", activeMasternodeInfo.service.ToString()));

    auto dmn = deterministicMNManager->GetListAtChainTip().GetMN(activeMasternodeInfo.proTxHash);
    if (dmn) {
        mnObj.push_back(Pair("proTxHash", dmn->proTxHash.ToString()));
        mnObj.push_back(Pair("collateralHash", dmn->collateralOutpoint.hash.ToString()));
        mnObj.push_back(Pair("collateralIndex", (int)dmn->collateralOutpoint.n));
        UniValue stateObj;
        dmn->pdmnState->ToJson(stateObj);
        mnObj.push_back(Pair("dmnState", stateObj));
    }
    mnObj.push_back(Pair("state", activeMasternodeManager->GetStateString()));
    mnObj.push_back(Pair("status", activeMasternodeManager->GetStatus()));

    return mnObj;
}

void masternode_winners_help()
{
    throw std::runtime_error(
            "masternode winners ( count \"filter\" )\n"
            "Print list of masternode winners\n"
            "\nArguments:\n"
            "1. count        (numeric, optional) number of last winners to return\n"
            "2. filter       (string, optional) filter for returned winners\n"
        );
}

UniValue masternode_winners(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 3)
        masternode_winners_help();

    int nHeight;
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();
        if (!pindex) return NullUniValue;

        nHeight = pindex->nHeight;
    }

    int nLast = 10;
    std::string strFilter = "";

    if (!request.params[1].isNull()) {
        nLast = atoi(request.params[1].get_str());
    }

    if (!request.params[2].isNull()) {
        strFilter = request.params[2].get_str();
    }

    UniValue obj(UniValue::VOBJ);
    auto mapPayments = GetRequiredPaymentsStrings(nHeight - nLast, nHeight + 20);
    for (const auto &p : mapPayments) {
        obj.push_back(Pair(strprintf("%d", p.first), p.second));
    }

    return obj;
}

[[ noreturn ]] void masternode_help()
{
    throw std::runtime_error(
        "masternode \"command\" ...\n"
        "Set of commands to execute masternode related actions\n"
        "\nArguments:\n"
        "1. \"command\"        (string or set of strings, required) The command to execute\n"
        "\nAvailable commands:\n"
        "  count        - Get information about number of masternodes (DEPRECATED options: 'total', 'ps', 'enabled', 'qualify', 'all')\n"
        "  current      - Print info on current masternode winner to be paid the next block (calculated locally)\n"
#ifdef ENABLE_WALLET
        "  outputs      - Print masternode compatible outputs\n"
#endif // ENABLE_WALLET
        "  status       - Print masternode status information\n"
        "  list         - Print list of all known masternodes (see masternodelist for more info)\n"
        "  winner       - Print info on next masternode winner to vote for\n"
        "  winners      - Print list of masternode winners\n"
        );
}

UniValue masternode(const JSONRPCRequest& request)
{
    std::string strCommand;
    if (!request.params[0].isNull()) {
        strCommand = request.params[0].get_str();
    }

    if (request.fHelp && strCommand.empty()) {
        masternode_help();
    }

    if (strCommand == "list") {
        return masternode_list(request);
    } else if (strCommand == "connect") {
        return masternode_connect(request);
    } else if (strCommand == "count") {
        return masternode_count(request);
    } else if (strCommand == "current") {
        return masternode_current(request);
    } else if (strCommand == "winner") {
        return masternode_winner(request);
#ifdef ENABLE_WALLET
    } else if (strCommand == "outputs") {
        return masternode_outputs(request);
#endif // ENABLE_WALLET
    } else if (strCommand == "status") {
        return masternode_status(request);
    } else if (strCommand == "winners") {
        return masternode_winners(request);
    } else {
        masternode_help();
    }
}

UniValue masternodelist(const JSONRPCRequest& request)
{
    std::string strMode = "json";
    std::string strFilter = "";

    if (!request.params[0].isNull()) strMode = request.params[0].get_str();
    if (!request.params[1].isNull()) strFilter = request.params[1].get_str();

    std::transform(strMode.begin(), strMode.end(), strMode.begin(), ::tolower);

    if (request.fHelp || (
                strMode != "addr" && strMode != "full" && strMode != "info" && strMode != "json" &&
                strMode != "owneraddress" && strMode != "votingaddress" &&
                strMode != "lastpaidtime" && strMode != "lastpaidblock" &&
                strMode != "payee" && strMode != "pubkeyoperator" &&
                strMode != "status"))
    {
        masternode_list_help();
    }

    UniValue obj(UniValue::VOBJ);

    auto mnList = deterministicMNManager->GetListAtChainTip();
    auto dmnToStatus = [&](const CDeterministicMNCPtr& dmn) {
        if (mnList.IsMNValid(dmn)) {
            return "ENABLED";
        }
        if (mnList.IsMNPoSeBanned(dmn)) {
            return "POSE_BANNED";
        }
        return "UNKNOWN";
    };
    auto dmnToLastPaidTime = [&](const CDeterministicMNCPtr& dmn) {
        if (dmn->pdmnState->nLastPaidHeight == 0) {
            return (int)0;
        }

        LOCK(cs_main);
        const CBlockIndex* pindex = chainActive[dmn->pdmnState->nLastPaidHeight];
        return (int)pindex->nTime;
    };

    mnList.ForEachMN(false, [&](const CDeterministicMNCPtr& dmn) {
        std::string strOutpoint = dmn->collateralOutpoint.ToStringShort();
        Coin coin;
        std::string collateralAddressStr = "UNKNOWN";
        if (GetUTXOCoin(dmn->collateralOutpoint, coin)) {
            CTxDestination collateralDest;
            if (ExtractDestination(coin.out.scriptPubKey, collateralDest)) {
                collateralAddressStr = EncodeDestination(collateralDest);
            }
        }

        CScript payeeScript = dmn->pdmnState->scriptPayout;
        CTxDestination payeeDest;
        std::string payeeStr = "UNKNOWN";
        if (ExtractDestination(payeeScript, payeeDest)) {
            payeeStr = EncodeDestination(payeeDest);
        }

        if (strMode == "addr") {
            std::string strAddress = dmn->pdmnState->addr.ToString(false);
            if (strFilter !="" && strAddress.find(strFilter) == std::string::npos &&
                strOutpoint.find(strFilter) == std::string::npos) return;
            obj.push_back(Pair(strOutpoint, strAddress));
        } else if (strMode == "full") {
            std::ostringstream streamFull;
            streamFull << std::setw(18) <<
                           dmnToStatus(dmn) << " " <<
                           payeeStr << " " << std::setw(10) <<
                           dmnToLastPaidTime(dmn) << " "  << std::setw(6) <<
                           dmn->pdmnState->nLastPaidHeight << " " <<
                           dmn->pdmnState->addr.ToString();
            std::string strFull = streamFull.str();
            if (strFilter !="" && strFull.find(strFilter) == std::string::npos &&
                strOutpoint.find(strFilter) == std::string::npos) return;
            obj.push_back(Pair(strOutpoint, strFull));
        } else if (strMode == "info") {
            std::ostringstream streamInfo;
            streamInfo << std::setw(18) <<
                           dmnToStatus(dmn) << " " <<
                           payeeStr << " " <<
                           dmn->pdmnState->addr.ToString();
            std::string strInfo = streamInfo.str();
            if (strFilter !="" && strInfo.find(strFilter) == std::string::npos &&
                strOutpoint.find(strFilter) == std::string::npos) return;
            obj.push_back(Pair(strOutpoint, strInfo));
        } else if (strMode == "json") {
            std::ostringstream streamInfo;
            streamInfo <<  dmn->proTxHash.ToString() << " " <<
                           dmn->pdmnState->addr.ToString() << " " <<
                           payeeStr << " " <<
                           dmnToStatus(dmn) << " " <<
                           dmnToLastPaidTime(dmn) << " " <<
                           dmn->pdmnState->nLastPaidHeight << " " <<
                           EncodeDestination(dmn->pdmnState->keyIDOwner) << " " <<
                           EncodeDestination(dmn->pdmnState->keyIDVoting) << " " <<
                           collateralAddressStr << " " <<
                           dmn->pdmnState->pubKeyOperator.Get().ToString();
            std::string strInfo = streamInfo.str();
            if (strFilter !="" && strInfo.find(strFilter) == std::string::npos &&
                strOutpoint.find(strFilter) == std::string::npos) return;
            UniValue objMN(UniValue::VOBJ);
            objMN.push_back(Pair("proTxHash", dmn->proTxHash.ToString()));
            objMN.push_back(Pair("address", dmn->pdmnState->addr.ToString()));
            objMN.push_back(Pair("payee", payeeStr));
            objMN.push_back(Pair("status", dmnToStatus(dmn)));
            objMN.push_back(Pair("lastpaidtime", dmnToLastPaidTime(dmn)));
            objMN.push_back(Pair("lastpaidblock", dmn->pdmnState->nLastPaidHeight));
            objMN.push_back(Pair("owneraddress", EncodeDestination(dmn->pdmnState->keyIDOwner)));
            objMN.push_back(Pair("votingaddress", EncodeDestination(dmn->pdmnState->keyIDVoting)));
            objMN.push_back(Pair("collateraladdress", collateralAddressStr));
            objMN.push_back(Pair("pubkeyoperator", dmn->pdmnState->pubKeyOperator.Get().ToString()));
            obj.push_back(Pair(strOutpoint, objMN));
        } else if (strMode == "lastpaidblock") {
            if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) return;
            obj.push_back(Pair(strOutpoint, dmn->pdmnState->nLastPaidHeight));
        } else if (strMode == "lastpaidtime") {
            if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) return;
            obj.push_back(Pair(strOutpoint, dmnToLastPaidTime(dmn)));
        } else if (strMode == "payee") {
            if (strFilter !="" && payeeStr.find(strFilter) == std::string::npos &&
                strOutpoint.find(strFilter) == std::string::npos) return;
            obj.push_back(Pair(strOutpoint, payeeStr));
        } else if (strMode == "owneraddress") {
            if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) return;
            obj.push_back(Pair(strOutpoint, EncodeDestination(dmn->pdmnState->keyIDOwner)));
        } else if (strMode == "pubkeyoperator") {
            if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) return;
            obj.push_back(Pair(strOutpoint, dmn->pdmnState->pubKeyOperator.Get().ToString()));
        } else if (strMode == "status") {
            std::string strStatus = dmnToStatus(dmn);
            if (strFilter !="" && strStatus.find(strFilter) == std::string::npos &&
                strOutpoint.find(strFilter) == std::string::npos) return;
            obj.push_back(Pair(strOutpoint, strStatus));
        } else if (strMode == "votingaddress") {
            if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) return;
            obj.push_back(Pair(strOutpoint, EncodeDestination(dmn->pdmnState->keyIDVoting)));
        }
    });

    return obj;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "axe",               "masternode",             &masternode,             {} },
    { "axe",               "masternodelist",         &masternodelist,         {} },
};

void RegisterMasternodeRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
