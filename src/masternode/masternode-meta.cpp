// Copyright (c) 2014-2019 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "masternode-meta.h"

CMasternodeMetaMan mmetaman;

const std::string CMasternodeMetaMan::SERIALIZATION_VERSION_STRING = "CMasternodeMetaMan-Version-1";

void CMasternodeMetaInfo::AddGovernanceVote(const uint256& nGovernanceObjectHash)
{
    LOCK(cs);
    // Insert a zero value, or not. Then increment the value regardless. This
    // ensures the value is in the map.
    const auto& pair = mapGovernanceObjectsVotedOn.emplace(nGovernanceObjectHash, 0);
    pair.first->second++;
}

void CMasternodeMetaInfo::RemoveGovernanceObject(const uint256& nGovernanceObjectHash)
{
    LOCK(cs);
    // Whether or not the govobj hash exists in the map first is irrelevant.
    mapGovernanceObjectsVotedOn.erase(nGovernanceObjectHash);
}

/**
*   FLAG GOVERNANCE ITEMS AS DIRTY
*
*   - When masternode come and go on the network, we must flag the items they voted on to recalc it's cached flags
*
*/
void CMasternodeMetaInfo::FlagGovernanceItemsAsDirty()
{
    LOCK(cs);
    for (auto& govObjHashPair : mapGovernanceObjectsVotedOn) {
        mmetaman.AddDirtyGovernanceObjectHash(govObjHashPair.first);
    }
}

CMasternodeMetaInfoPtr CMasternodeMetaMan::GetMetaInfo(const uint256& proTxHash, bool fCreate)
{
    LOCK(cs);
    auto it = metaInfos.find(proTxHash);
    if (it != metaInfos.end()) {
        return it->second;
    }
    if (!fCreate) {
        return nullptr;
    }
    it = metaInfos.emplace(proTxHash, std::make_shared<CMasternodeMetaInfo>(proTxHash)).first;
    return it->second;
}

void CMasternodeMetaMan::AllowMixing(const uint256& proTxHash)
{
    LOCK(cs);
    auto mm = GetMetaInfo(proTxHash);
    nDsqCount++;
    LOCK(mm->cs);
    mm->nLastDsq = nDsqCount;
    mm->nMixingTxCount = 0;
}

void CMasternodeMetaMan::DisallowMixing(const uint256& proTxHash)
{
    LOCK(cs);
    auto mm = GetMetaInfo(proTxHash);

    LOCK(mm->cs);
    mm->nMixingTxCount++;
}

bool CMasternodeMetaMan::AddGovernanceVote(const uint256& proTxHash, const uint256& nGovernanceObjectHash)
{
    LOCK(cs);
    auto mm = GetMetaInfo(proTxHash);
    mm->AddGovernanceVote(nGovernanceObjectHash);
    return true;
}

void CMasternodeMetaMan::RemoveGovernanceObject(const uint256& nGovernanceObjectHash)
{
    LOCK(cs);
    for(auto& p : metaInfos) {
        p.second->RemoveGovernanceObject(nGovernanceObjectHash);
    }
}

void CMasternodeMetaMan::AddDirtyGovernanceObjectHash(const uint256& nHash)
{
    LOCK(cs);
    vecDirtyGovernanceObjectHashes.push_back(nHash);
}

std::vector<uint256> CMasternodeMetaMan::GetAndClearDirtyGovernanceObjectHashes()
{
    LOCK(cs);
    std::vector<uint256> vecTmp = std::move(vecDirtyGovernanceObjectHashes);
    vecDirtyGovernanceObjectHashes.clear();
    return vecTmp;
}

void CMasternodeMetaMan::Clear()
{
    LOCK(cs);
    metaInfos.clear();
    vecDirtyGovernanceObjectHashes.clear();
}

void CMasternodeMetaMan::CheckAndRemove()
{

}

std::string CMasternodeMetaMan::ToString() const
{
    std::ostringstream info;

    info << "Masternodes: meta infos object count: " << (int)metaInfos.size() <<
         ", nDsqCount: " << (int)nDsqCount;
    return info.str();
}
