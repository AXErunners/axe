// Copyright (c) 2018-2019 The Axe Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef AXE_QUORUMS_UTILS_H
#define AXE_QUORUMS_UTILS_H

#include <consensus/params.h>
#include <net.h>

#include <evo/deterministicmns.h>

#include <vector>

namespace llmq
{

class CLLMQUtils
{
public:
    // includes members which failed DKG
    static std::vector<CDeterministicMNCPtr> GetAllQuorumMembers(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum);

    static uint256 BuildCommitmentHash(Consensus::LLMQType llmqType, const uint256& blockHash, const std::vector<bool>& validMembers, const CBLSPublicKey& pubKey, const uint256& vvecHash);
    static uint256 BuildSignHash(Consensus::LLMQType llmqType, const uint256& quorumHash, const uint256& id, const uint256& msgHash);

    // works for sig shares and recovered sigs
    template<typename T>
    static uint256 BuildSignHash(const T& s)
    {
        return BuildSignHash((Consensus::LLMQType)s.llmqType, s.quorumHash, s.id, s.msgHash);
    }

    static uint256 DeterministicOutboundConnection(const uint256& proTxHash1, const uint256& proTxHash2);
    static std::set<uint256> GetQuorumConnections(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, const uint256& forMember, bool onlyOutbound);
    static std::set<uint256> GetQuorumRelayMembers(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, const uint256& forMember, bool onlyOutbound);
    static std::set<size_t> CalcDeterministicWatchConnections(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, size_t memberCount, size_t connectionCount);

    static void EnsureQuorumConnections(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, const uint256& myProTxHash, bool allowWatch);
    static void AddQuorumProbeConnections(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, const uint256& myProTxHash);

    static bool IsQuorumActive(Consensus::LLMQType llmqType, const uint256& quorumHash);

    template<typename NodesContainer, typename Continue, typename Callback>
    static void IterateNodesRandom(NodesContainer& nodeStates, Continue&& cont, Callback&& callback, FastRandomContext& rnd)
    {
        std::vector<typename NodesContainer::iterator> rndNodes;
        rndNodes.reserve(nodeStates.size());
        for (auto it = nodeStates.begin(); it != nodeStates.end(); ++it) {
            rndNodes.emplace_back(it);
        }
        if (rndNodes.empty()) {
            return;
        }
        std::random_shuffle(rndNodes.begin(), rndNodes.end(), rnd);

        size_t idx = 0;
        while (!rndNodes.empty() && cont()) {
            auto nodeId = rndNodes[idx]->first;
            auto& ns = rndNodes[idx]->second;

            if (callback(nodeId, ns)) {
                idx = (idx + 1) % rndNodes.size();
            } else {
                rndNodes.erase(rndNodes.begin() + idx);
                if (rndNodes.empty()) {
                    break;
                }
                idx %= rndNodes.size();
            }
        }
    }
    static std::string ToHexStr(const std::vector<bool>& vBits)
    {
        std::vector<uint8_t> vBytes((vBits.size() + 7) / 8);
        for (size_t i = 0; i < vBits.size(); i++) {
            vBytes[i / 8] |= vBits[i] << (i % 8);
        }
        return HexStr(vBytes);
    }
};

} // namespace llmq

#endif//AXE_QUORUMS_UTILS_H
