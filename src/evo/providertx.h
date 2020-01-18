// Copyright (c) 2018-2020 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef AXE_PROVIDERTX_H
#define AXE_PROVIDERTX_H

#include "bls/bls.h"
#include "consensus/validation.h"
#include "primitives/transaction.h"

#include "base58.h"
#include "netaddress.h"
#include "pubkey.h"
#include "univalue.h"

class CBlockIndex;

class CProRegTx
{
public:
    static const uint16_t CURRENT_VERSION = 1;

public:
    uint16_t nVersion{CURRENT_VERSION};                    // message version
    uint16_t nType{0};                                     // only 0 supported for now
    uint16_t nMode{0};                                     // only 0 supported for now
    COutPoint collateralOutpoint{uint256(), (uint32_t)-1}; // if hash is null, we refer to a ProRegTx output
    CService addr;
    CKeyID keyIDOwner;
    CBLSPublicKey pubKeyOperator;
    CKeyID keyIDVoting;
    uint16_t nOperatorReward{0};
    CScript scriptPayout;
    uint256 inputsHash; // replay protection
    std::vector<unsigned char> vchSig;

public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(nType);
        READWRITE(nMode);
        READWRITE(collateralOutpoint);
        READWRITE(addr);
        READWRITE(keyIDOwner);
        READWRITE(pubKeyOperator);
        READWRITE(keyIDVoting);
        READWRITE(nOperatorReward);
        READWRITE(scriptPayout);
        READWRITE(inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(vchSig);
        }
    }

    // When signing with the collateral key, we don't sign the hash but a generated message instead
    // This is needed for HW wallet support which can only sign text messages as of now
    std::string MakeSignString() const;

    std::string ToString() const;

    void ToJson(UniValue& obj) const
    {
        obj.clear();
        obj.setObject();
        obj.push_back(Pair("version", nVersion));
        obj.push_back(Pair("collateralHash", collateralOutpoint.hash.ToString()));
        obj.push_back(Pair("collateralIndex", (int)collateralOutpoint.n));
        obj.push_back(Pair("service", addr.ToString(false)));
        obj.push_back(Pair("ownerAddress", CBitcoinAddress(keyIDOwner).ToString()));
        obj.push_back(Pair("votingAddress", CBitcoinAddress(keyIDVoting).ToString()));

        CTxDestination dest;
        if (ExtractDestination(scriptPayout, dest)) {
            CBitcoinAddress bitcoinAddress(dest);
            obj.push_back(Pair("payoutAddress", bitcoinAddress.ToString()));
        }
        obj.push_back(Pair("pubKeyOperator", pubKeyOperator.ToString()));
        obj.push_back(Pair("operatorReward", (double)nOperatorReward / 100));

        obj.push_back(Pair("inputsHash", inputsHash.ToString()));
    }
};

class CProUpServTx
{
public:
    static const uint16_t CURRENT_VERSION = 1;

public:
    uint16_t nVersion{CURRENT_VERSION}; // message version
    uint256 proTxHash;
    CService addr;
    CScript scriptOperatorPayout;
    uint256 inputsHash; // replay protection
    CBLSSignature sig;

public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(proTxHash);
        READWRITE(addr);
        READWRITE(scriptOperatorPayout);
        READWRITE(inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(sig);
        }
    }

public:
    std::string ToString() const;

    void ToJson(UniValue& obj) const
    {
        obj.clear();
        obj.setObject();
        obj.push_back(Pair("version", nVersion));
        obj.push_back(Pair("proTxHash", proTxHash.ToString()));
        obj.push_back(Pair("service", addr.ToString(false)));
        CTxDestination dest;
        if (ExtractDestination(scriptOperatorPayout, dest)) {
            CBitcoinAddress bitcoinAddress(dest);
            obj.push_back(Pair("operatorPayoutAddress", bitcoinAddress.ToString()));
        }
        obj.push_back(Pair("inputsHash", inputsHash.ToString()));
    }
};

class CProUpRegTx
{
public:
    static const uint16_t CURRENT_VERSION = 1;

public:
    uint16_t nVersion{CURRENT_VERSION}; // message version
    uint256 proTxHash;
    uint16_t nMode{0}; // only 0 supported for now
    CBLSPublicKey pubKeyOperator;
    CKeyID keyIDVoting;
    CScript scriptPayout;
    uint256 inputsHash; // replay protection
    std::vector<unsigned char> vchSig;

public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(proTxHash);
        READWRITE(nMode);
        READWRITE(pubKeyOperator);
        READWRITE(keyIDVoting);
        READWRITE(scriptPayout);
        READWRITE(inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(vchSig);
        }
    }

public:
    std::string ToString() const;

    void ToJson(UniValue& obj) const
    {
        obj.clear();
        obj.setObject();
        obj.push_back(Pair("version", nVersion));
        obj.push_back(Pair("proTxHash", proTxHash.ToString()));
        obj.push_back(Pair("votingAddress", CBitcoinAddress(keyIDVoting).ToString()));
        CTxDestination dest;
        if (ExtractDestination(scriptPayout, dest)) {
            CBitcoinAddress bitcoinAddress(dest);
            obj.push_back(Pair("payoutAddress", bitcoinAddress.ToString()));
        }
        obj.push_back(Pair("pubKeyOperator", pubKeyOperator.ToString()));
        obj.push_back(Pair("inputsHash", inputsHash.ToString()));
    }
};

class CProUpRevTx
{
public:
    static const uint16_t CURRENT_VERSION = 1;

    // these are just informational and do not have any effect on the revocation
    enum {
        REASON_NOT_SPECIFIED = 0,
        REASON_TERMINATION_OF_SERVICE = 1,
        REASON_COMPROMISED_KEYS = 2,
        REASON_CHANGE_OF_KEYS = 3,
        REASON_LAST = REASON_CHANGE_OF_KEYS
    };

public:
    uint16_t nVersion{CURRENT_VERSION}; // message version
    uint256 proTxHash;
    uint16_t nReason{REASON_NOT_SPECIFIED};
    uint256 inputsHash; // replay protection
    CBLSSignature sig;

public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(proTxHash);
        READWRITE(nReason);
        READWRITE(inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(sig);
        }
    }

public:
    std::string ToString() const;

    void ToJson(UniValue& obj) const
    {
        obj.clear();
        obj.setObject();
        obj.push_back(Pair("version", nVersion));
        obj.push_back(Pair("proTxHash", proTxHash.ToString()));
        obj.push_back(Pair("reason", (int)nReason));
        obj.push_back(Pair("inputsHash", inputsHash.ToString()));
    }
};


bool CheckProRegTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state);
bool CheckProUpServTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state);
bool CheckProUpRegTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state);
bool CheckProUpRevTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state);

#endif //AXE_PROVIDERTX_H
