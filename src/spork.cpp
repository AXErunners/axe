// Copyright (c) 2014-2019 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <spork.h>

#include <base58.h>
#include <chainparams.h>
#include <validation.h>
#include <messagesigner.h>
#include <net_processing.h>
#include <netmessagemaker.h>

#include <string>

const std::string CSporkManager::SERIALIZATION_VERSION_STRING = "CSporkManager-Version-2";

// Mainnet
#define MAKE_SPORK_DEF_M(name, defaultValue) CSporkDefM{name, defaultValue, #name}
std::vector<CSporkDefM> sporkDefsM = {
    MAKE_SPORK_DEF_M(SPORK_2_INSTANTSEND_ENABLED,            0),             // ON
    MAKE_SPORK_DEF_M(SPORK_3_INSTANTSEND_BLOCK_FILTERING,    0),             // ON
    MAKE_SPORK_DEF_M(SPORK_9_SUPERBLOCKS_ENABLED,            0),             // ON
    MAKE_SPORK_DEF_M(SPORK_17_QUORUM_DKG_ENABLED,            0),             // ON
    MAKE_SPORK_DEF_M(SPORK_19_CHAINLOCKS_ENABLED,            0),             // ON
    MAKE_SPORK_DEF_M(SPORK_21_QUORUM_ALL_CONNECTED,          4070908800ULL), // OFF
    MAKE_SPORK_DEF_M(SPORK_22_PS_MORE_PARTICIPANTS,          4070908800ULL), // OFF
};

// Testnet
#define MAKE_SPORK_DEF_T(name, defaultValue) CSporkDefT{name, defaultValue, #name}
std::vector<CSporkDefT> sporkDefsT = {
    MAKE_SPORK_DEF_T(SPORK_2_INSTANTSEND_ENABLED,            0),             // ON
    MAKE_SPORK_DEF_T(SPORK_3_INSTANTSEND_BLOCK_FILTERING,    0),             // ON
    MAKE_SPORK_DEF_T(SPORK_9_SUPERBLOCKS_ENABLED,            0),             // ON
    MAKE_SPORK_DEF_T(SPORK_17_QUORUM_DKG_ENABLED,            0),             // ON
    MAKE_SPORK_DEF_T(SPORK_19_CHAINLOCKS_ENABLED,            0),             // ON
    MAKE_SPORK_DEF_T(SPORK_21_QUORUM_ALL_CONNECTED,          4070908800ULL), // OFF
    MAKE_SPORK_DEF_T(SPORK_22_PS_MORE_PARTICIPANTS,          4070908800ULL), // OFF
};

// Regtest
#define MAKE_SPORK_DEF_R(name, defaultValue) CSporkDefR{name, defaultValue, #name}
std::vector<CSporkDefR> sporkDefsR = {
    MAKE_SPORK_DEF_R(SPORK_2_INSTANTSEND_ENABLED,            4070908800ULL), // OFF
    MAKE_SPORK_DEF_R(SPORK_3_INSTANTSEND_BLOCK_FILTERING,    4070908800ULL), // OFF
    MAKE_SPORK_DEF_R(SPORK_9_SUPERBLOCKS_ENABLED,            4070908800ULL), // OFF
    MAKE_SPORK_DEF_R(SPORK_17_QUORUM_DKG_ENABLED,            4070908800ULL), // OFF
    MAKE_SPORK_DEF_R(SPORK_19_CHAINLOCKS_ENABLED,            4070908800ULL), // OFF
    MAKE_SPORK_DEF_R(SPORK_21_QUORUM_ALL_CONNECTED,          4070908800ULL), // OFF
    MAKE_SPORK_DEF_R(SPORK_22_PS_MORE_PARTICIPANTS,          4070908800ULL), // OFF
};

// Devnet
#define MAKE_SPORK_DEF_D(name, defaultValue) CSporkDefD{name, defaultValue, #name}
std::vector<CSporkDefD> sporkDefsD = {
    MAKE_SPORK_DEF_D(SPORK_2_INSTANTSEND_ENABLED,            4070908800ULL), // OFF
    MAKE_SPORK_DEF_D(SPORK_3_INSTANTSEND_BLOCK_FILTERING,    4070908800ULL), // OFF
    MAKE_SPORK_DEF_D(SPORK_9_SUPERBLOCKS_ENABLED,            4070908800ULL), // OFF
    MAKE_SPORK_DEF_D(SPORK_17_QUORUM_DKG_ENABLED,            4070908800ULL), // OFF
    MAKE_SPORK_DEF_D(SPORK_19_CHAINLOCKS_ENABLED,            4070908800ULL), // OFF
    MAKE_SPORK_DEF_D(SPORK_21_QUORUM_ALL_CONNECTED,          4070908800ULL), // OFF
    MAKE_SPORK_DEF_D(SPORK_22_PS_MORE_PARTICIPANTS,          4070908800ULL), // OFF
};

CSporkManager sporkManager;

CSporkManager::CSporkManager()
{
    for (auto& sporkDefM : sporkDefsM) {
        sporkDefsMById.emplace(sporkDefM.sporkId, &sporkDefM);
        sporkDefsMByName.emplace(sporkDefM.name, &sporkDefM);
    };

    for (auto& sporkDefT : sporkDefsT) {
        sporkDefsTById.emplace(sporkDefT.sporkId, &sporkDefT);
        sporkDefsTByName.emplace(sporkDefT.name, &sporkDefT);
    };

    for (auto& sporkDefR : sporkDefsR) {
        sporkDefsRById.emplace(sporkDefR.sporkId, &sporkDefR);
        sporkDefsRByName.emplace(sporkDefR.name, &sporkDefR);
    };

    for (auto& sporkDefD : sporkDefsD) {
        sporkDefsDById.emplace(sporkDefD.sporkId, &sporkDefD);
        sporkDefsDByName.emplace(sporkDefD.name, &sporkDefD);
    }
}

bool CSporkManager::SporkValueIsActive(SporkId nSporkID, int64_t &nActiveValueRet) const
{
    LOCK(cs);

    if (!mapSporksActive.count(nSporkID)) return false;

    // calc how many values we have and how many signers vote for every value
    std::unordered_map<int64_t, int> mapValueCounts;
    for (const auto& pair: mapSporksActive.at(nSporkID)) {
        mapValueCounts[pair.second.nValue]++;
        if (mapValueCounts.at(pair.second.nValue) >= nMinSporkKeys) {
            // nMinSporkKeys is always more than the half of the max spork keys number,
            // so there is only one such value and we can stop here
            nActiveValueRet = pair.second.nValue;
            return true;
        }
    }

    return false;
}

void CSporkManager::Clear()
{
    LOCK(cs);
    mapSporksActive.clear();
    mapSporksByHash.clear();
    // sporkPubKeyID and sporkPrivKey should be set in init.cpp,
    // we should not alter them here.
}

void CSporkManager::CheckAndRemove()
{
    LOCK(cs);
    bool fSporkAddressIsSet = !setSporkPubKeyIDs.empty();
    assert(fSporkAddressIsSet);

    auto itActive = mapSporksActive.begin();
    while (itActive != mapSporksActive.end()) {
        auto itSignerPair = itActive->second.begin();
        while (itSignerPair != itActive->second.end()) {
            if (setSporkPubKeyIDs.find(itSignerPair->first) == setSporkPubKeyIDs.end()) {
                mapSporksByHash.erase(itSignerPair->second.GetHash());
                continue;
            }
            if (!itSignerPair->second.CheckSignature(itSignerPair->first)) {
                mapSporksByHash.erase(itSignerPair->second.GetHash());
                itActive->second.erase(itSignerPair++);
                continue;
            }
            ++itSignerPair;
        }
        if (itActive->second.empty()) {
            mapSporksActive.erase(itActive++);
            continue;
        }
        ++itActive;
    }

    auto itByHash = mapSporksByHash.begin();
    while (itByHash != mapSporksByHash.end()) {
        bool found = false;
        for (const auto& signer: setSporkPubKeyIDs) {
            if (itByHash->second.CheckSignature(signer)) {
                found = true;
                break;
            }
        }
        if (!found) {
            mapSporksByHash.erase(itByHash++);
            continue;
        }
        ++itByHash;
    }
}

void CSporkManager::ProcessSpork(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman& connman)
{

    if (strCommand == NetMsgType::SPORK) {

        CSporkMessage spork;
        vRecv >> spork;

        uint256 hash = spork.GetHash();

        std::string strLogMsg;
        {
            LOCK(cs_main);
            EraseObjectRequest(pfrom->GetId(), CInv(MSG_SPORK, hash));
            if(!chainActive.Tip()) return;
            strLogMsg = strprintf("SPORK -- hash: %s id: %d value: %10d bestHeight: %d peer=%d", hash.ToString(), spork.nSporkID, spork.nValue, chainActive.Height(), pfrom->GetId());
        }

        if (spork.nTimeSigned > GetAdjustedTime() + 2 * 60 * 60) {
            LOCK(cs_main);
            LogPrint(BCLog::SPORK, "CSporkManager::ProcessSpork -- ERROR: too far into the future\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        CKeyID keyIDSigner;

        if (!spork.GetSignerKeyID(keyIDSigner) || !setSporkPubKeyIDs.count(keyIDSigner)) {
            LOCK(cs_main);
            LogPrint(BCLog::SPORK, "CSporkManager::ProcessSpork -- ERROR: invalid signature\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        {
            LOCK(cs); // make sure to not lock this together with cs_main
            if (mapSporksActive.count(spork.nSporkID)) {
                if (mapSporksActive[spork.nSporkID].count(keyIDSigner)) {
                    if (mapSporksActive[spork.nSporkID][keyIDSigner].nTimeSigned >= spork.nTimeSigned) {
                        LogPrint(BCLog::SPORK, "%s seen\n", strLogMsg);
                        return;
                    } else {
                        LogPrintf("%s updated\n", strLogMsg);
                    }
                } else {
                    LogPrintf("%s new signer\n", strLogMsg);
                }
            } else {
                LogPrintf("%s new\n", strLogMsg);
            }
        }


        {
            LOCK(cs); // make sure to not lock this together with cs_main
            mapSporksByHash[hash] = spork;
            mapSporksActive[spork.nSporkID][keyIDSigner] = spork;
        }
        spork.Relay(connman);

    } else if (strCommand == NetMsgType::GETSPORKS) {
        LOCK(cs); // make sure to not lock this together with cs_main
        for (const auto& pair : mapSporksActive) {
            for (const auto& signerSporkPair: pair.second) {
                connman.PushMessage(pfrom, CNetMsgMaker(pfrom->GetSendVersion()).Make(NetMsgType::SPORK, signerSporkPair.second));
            }
        }
    }

}

bool CSporkManager::UpdateSpork(SporkId nSporkID, int64_t nValue, CConnman& connman)
{
    CSporkMessage spork = CSporkMessage(nSporkID, nValue, GetAdjustedTime());

    LOCK(cs);

    if (!spork.Sign(sporkPrivKey)) {
        LogPrintf("CSporkManager::%s -- ERROR: signing failed for spork %d\n", __func__, nSporkID);
        return false;
    }

    CKeyID keyIDSigner;
    if (!spork.GetSignerKeyID(keyIDSigner) || !setSporkPubKeyIDs.count(keyIDSigner)) {
        LogPrintf("CSporkManager::UpdateSpork: failed to find keyid for private key\n");
        return false;
    }

    LogPrintf("CSporkManager::%s -- signed %d %s\n", __func__, nSporkID, spork.GetHash().ToString());

    mapSporksByHash[spork.GetHash()] = spork;
    mapSporksActive[nSporkID][keyIDSigner] = spork;

    spork.Relay(connman);
    return true;
}

bool CSporkManager::IsSporkActive(SporkId nSporkID)
{
    int64_t nSporkValue = GetSporkValue(nSporkID);
    return nSporkValue < GetAdjustedTime();
}

int64_t CSporkManager::GetSporkValue(SporkId nSporkID)
{
    LOCK(cs);

    int64_t nSporkValue = -1;
    if (SporkValueIsActive(nSporkID, nSporkValue)) {
        return nSporkValue;
    }

    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
      auto it = sporkDefsMById.find(nSporkID);
      if (it != sporkDefsMById.end()) {
        return it->second->defaultValue;
      }
    };
    
    if (Params().NetworkIDString() == CBaseChainParams::TESTNET) {
      auto it = sporkDefsTById.find(nSporkID);
      if (it != sporkDefsTById.end()) {
        return it->second->defaultValue;
      }
    };
    
    if (Params().NetworkIDString() == CBaseChainParams::REGTEST) {
      auto it = sporkDefsRById.find(nSporkID);
      if (it != sporkDefsRById.end()) {
        return it->second->defaultValue;
      }
    };
    
    if (Params().NetworkIDString() == CBaseChainParams::DEVNET) {
      auto it = sporkDefsDById.find(nSporkID);
      if (it != sporkDefsDById.end()) {
        return it->second->defaultValue;
      }
    }

    LogPrint(BCLog::SPORK, "CSporkManager::GetSporkValue -- Unknown Spork ID %d\n", nSporkID);
    return -1;
}

SporkId CSporkManager::GetSporkIDByName(const std::string& strName)
{
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
      auto it = sporkDefsMByName.find(strName);
      if (it == sporkDefsMByName.end()) {
          LogPrint(BCLog::SPORK, "CSporkManager::GetSporkIDByName -- Unknown Spork name '%s'\n", strName);
          return SPORK_INVALID;
      }
      return it->second->sporkId;
    };
    
    if (Params().NetworkIDString() == CBaseChainParams::TESTNET) {
      auto it = sporkDefsTByName.find(strName);
      if (it == sporkDefsTByName.end()) {
          LogPrint(BCLog::SPORK, "CSporkManager::GetSporkIDByName -- Unknown Spork name '%s'\n", strName);
          return SPORK_INVALID;
      }
      return it->second->sporkId;
    };
    
    if (Params().NetworkIDString() == CBaseChainParams::REGTEST) {
      auto it = sporkDefsRByName.find(strName);
      if (it == sporkDefsRByName.end()) {
          LogPrint(BCLog::SPORK, "CSporkManager::GetSporkIDByName -- Unknown Spork name '%s'\n", strName);
          return SPORK_INVALID;
      }
      return it->second->sporkId;
    };
    
    if (Params().NetworkIDString() == CBaseChainParams::DEVNET) {
      auto it = sporkDefsDByName.find(strName);
      if (it == sporkDefsDByName.end()) {
          LogPrint(BCLog::SPORK, "CSporkManager::GetSporkIDByName -- Unknown Spork name '%s'\n", strName);
          return SPORK_INVALID;
      }
      return it->second->sporkId;
    };
}

std::string CSporkManager::GetSporkNameByID(SporkId nSporkID)
{
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
      auto it = sporkDefsMById.find(nSporkID);
      if (it == sporkDefsMById.end()) {
          LogPrint(BCLog::SPORK, "CSporkManager::GetSporkNameByID -- Unknown Spork ID %d\n", nSporkID);
          return "Unknown";
      }
      return it->second->name;
    };
    
    if (Params().NetworkIDString() == CBaseChainParams::TESTNET) {
      auto it = sporkDefsTById.find(nSporkID);
      if (it == sporkDefsTById.end()) {
          LogPrint(BCLog::SPORK, "CSporkManager::GetSporkNameByID -- Unknown Spork ID %d\n", nSporkID);
          return "Unknown";
      }
      return it->second->name;
    };
    
    if (Params().NetworkIDString() == CBaseChainParams::REGTEST) {
      auto it = sporkDefsRById.find(nSporkID);
      if (it == sporkDefsRById.end()) {
          LogPrint(BCLog::SPORK, "CSporkManager::GetSporkNameByID -- Unknown Spork ID %d\n", nSporkID);
          return "Unknown";
      }
      return it->second->name;
    };
    
    if (Params().NetworkIDString() == CBaseChainParams::DEVNET) {
      auto it = sporkDefsDById.find(nSporkID);
      if (it == sporkDefsDById.end()) {
          LogPrint(BCLog::SPORK, "CSporkManager::GetSporkNameByID -- Unknown Spork ID %d\n", nSporkID);
          return "Unknown";
      }
      return it->second->name;
    };
}

bool CSporkManager::GetSporkByHash(const uint256& hash, CSporkMessage &sporkRet)
{
    LOCK(cs);

    const auto it = mapSporksByHash.find(hash);

    if (it == mapSporksByHash.end())
        return false;

    sporkRet = it->second;

    return true;
}

bool CSporkManager::SetSporkAddress(const std::string& strAddress) {
    LOCK(cs);
    CTxDestination dest = DecodeDestination(strAddress);
    const CKeyID *keyID = boost::get<CKeyID>(&dest);
    if (!keyID) {
        LogPrintf("CSporkManager::SetSporkAddress -- Failed to parse spork address\n");
        return false;
    }
    setSporkPubKeyIDs.insert(*keyID);
    return true;
}

bool CSporkManager::SetMinSporkKeys(int minSporkKeys)
{
    int maxKeysNumber = setSporkPubKeyIDs.size();
    if ((minSporkKeys <= maxKeysNumber / 2) || (minSporkKeys > maxKeysNumber)) {
        LogPrintf("CSporkManager::SetMinSporkKeys -- Invalid min spork signers number: %d\n", minSporkKeys);
        return false;
    }
    nMinSporkKeys = minSporkKeys;
    return true;
}

bool CSporkManager::SetPrivKey(const std::string& strPrivKey)
{
    CKey key;
    CPubKey pubKey;
    if(!CMessageSigner::GetKeysFromSecret(strPrivKey, key, pubKey)) {
        LogPrintf("CSporkManager::SetPrivKey -- Failed to parse private key\n");
        return false;
    }

    if (setSporkPubKeyIDs.find(pubKey.GetID()) == setSporkPubKeyIDs.end()) {
        LogPrintf("CSporkManager::SetPrivKey -- New private key does not belong to spork addresses\n");
        return false;
    }

    CSporkMessage spork;
    if (!spork.Sign(key)) {
        LogPrintf("CSporkManager::SetPrivKey -- Test signing failed\n");
        return false;
    }

    // Test signing successful, proceed
    LOCK(cs);
    LogPrintf("CSporkManager::SetPrivKey -- Successfully initialized as spork signer\n");
    sporkPrivKey = key;
    return true;
}

std::string CSporkManager::ToString() const
{
    LOCK(cs);
    return strprintf("Sporks: %llu", mapSporksActive.size());
}

uint256 CSporkMessage::GetHash() const
{
    return SerializeHash(*this);
}

uint256 CSporkMessage::GetSignatureHash() const
{
    CHashWriter s(SER_GETHASH, 0);
    s << nSporkID;
    s << nValue;
    s << nTimeSigned;
    return s.GetHash();
}

bool CSporkMessage::Sign(const CKey& key)
{
    if (!key.IsValid()) {
        LogPrintf("CSporkMessage::Sign -- signing key is not valid\n");
        return false;
    }

    CKeyID pubKeyId = key.GetPubKey().GetID();
    std::string strError = "";

    // Harden Spork6 so that it is active on testnet and no other networks
    if (Params().NetworkIDString() == CBaseChainParams::TESTNET) {
        uint256 hash = GetSignatureHash();

        if(!CHashSigner::SignHash(hash, key, vchSig)) {
            LogPrintf("CSporkMessage::Sign -- SignHash() failed\n");
            return false;
        }

        if (!CHashSigner::VerifyHash(hash, pubKeyId, vchSig, strError)) {
            LogPrintf("CSporkMessage::Sign -- VerifyHash() failed, error: %s\n", strError);
            return false;
        }
    } else {
        std::string strMessage = std::to_string(nSporkID) + std::to_string(nValue) + std::to_string(nTimeSigned);

        if(!CMessageSigner::SignMessage(strMessage, vchSig, key)) {
            LogPrintf("CSporkMessage::Sign -- SignMessage() failed\n");
            return false;
        }

        if(!CMessageSigner::VerifyMessage(pubKeyId, vchSig, strMessage, strError)) {
            LogPrintf("CSporkMessage::Sign -- VerifyMessage() failed, error: %s\n", strError);
            return false;
        }
    }

    return true;
}

bool CSporkMessage::CheckSignature(const CKeyID& pubKeyId) const
{
    std::string strError = "";

    // Harden Spork6 so that it is active on testnet and no other networks
    if (Params().NetworkIDString() == CBaseChainParams::TESTNET) {
        uint256 hash = GetSignatureHash();

        if (!CHashSigner::VerifyHash(hash, pubKeyId, vchSig, strError)) {
            LogPrint(BCLog::SPORK, "CSporkMessage::CheckSignature -- VerifyHash() failed, error: %s\n", strError);
            return false;
        }
    } else {
        std::string strMessage = std::to_string(nSporkID) + std::to_string(nValue) + std::to_string(nTimeSigned);

        if (!CMessageSigner::VerifyMessage(pubKeyId, vchSig, strMessage, strError)){
            LogPrint(BCLog::SPORK, "CSporkMessage::CheckSignature -- VerifyMessage() failed, error: %s\n", strError);
            return false;
        }
    }

    return true;
}

bool CSporkMessage::GetSignerKeyID(CKeyID &retKeyidSporkSigner)
{
    CPubKey pubkeyFromSig;
    // Harden Spork6 so that it is active on testnet and no other networks
    if (Params().NetworkIDString() == CBaseChainParams::TESTNET) {
        if (!pubkeyFromSig.RecoverCompact(GetSignatureHash(), vchSig)) {
            return false;
        }
    } else {
        std::string strMessage = std::to_string(nSporkID) + std::to_string(nValue) + std::to_string(nTimeSigned);
        CHashWriter ss(SER_GETHASH, 0);
        ss << strMessageMagic;
        ss << strMessage;
        if (!pubkeyFromSig.RecoverCompact(ss.GetHash(), vchSig)) {
            return false;
        }
    }

    retKeyidSporkSigner = pubkeyFromSig.GetID();
    return true;
}

void CSporkMessage::Relay(CConnman& connman)
{
    CInv inv(MSG_SPORK, GetHash());
    connman.RelayInv(inv);
}
