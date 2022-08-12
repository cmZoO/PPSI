#include "libPSI/PSI/Kkrt/KkrtPsiReceiver.h"
#include "libPSI/PSI/Kkrt/KkrtPsiSender.h"
#include "libOTe/NChooseOne/Kkrt/KkrtNcoOtReceiver.h"
#include "libOTe/NChooseOne/Kkrt/KkrtNcoOtSender.h"
#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Network/Session.h>
#include <cryptoTools/Network/Channel.h>

using namespace oc;

oc::block commSeed = oc::toBlock(123);
oc::PRNG commPrng(commSeed);

oc::KkrtNcoOtReceiver otRecv;
oc::KkrtPsiReceiver recvPSIs;
oc::KkrtNcoOtSender otSend;
oc::KkrtPsiSender sendPSIs;
oc::IOService osu_ios(0);
oc::Channel osu_chl;

void osu_init(oc::PRNG &prng) {
    oc::u8 dummy[1];
    if (party == 1) {
        oc::Session ep(osu_ios, address, port + osu_port, oc::EpMode::Client);
        osu_chl = ep.addChannel();
        osu_chl.recv(dummy, 1);
        osu_chl.asyncSend(dummy, 1);
        recvPSIs.init(serverSize, clientSize * hash_hash_num, stat_sec_param, osu_chl, otRecv, prng.get<oc::block>());
    } else {
        oc::Session ep(osu_ios, address, port + osu_port, oc::EpMode::Server);
        osu_chl = ep.addChannel();
        osu_chl.asyncSend(dummy, 1);
        osu_chl.recv(dummy, 1);
        sendPSIs.init(serverSize, clientSize * hash_hash_num, stat_sec_param, osu_chl, otSend, prng.get<oc::block>());
    }
    std::cout << "osu Channel&libPSI init Done" << std::endl;
}

void client_hash_step(std::vector<oc::block> &items, oc::Matrix<u64> &mItemToBinMap, u64 numBins) {
    std::array<oc::block, 8> hashs;
    oc::AES hasher(commSeed);

    auto mNumHashFunctions = mItemToBinMap.stride();
    auto mainSteps = items.size() / hashs.size();
    auto remSteps = items.size() % hashs.size();
    u64 itemIdx = 0;

    for (u64 i = 0; i < mainSteps; ++i, itemIdx += 8)
    {
        hasher.ecbEncBlocks(items.data() + itemIdx, 8, hashs.data());
        auto itemIdx0 = itemIdx + 0;
        auto itemIdx1 = itemIdx + 1;
        auto itemIdx2 = itemIdx + 2;
        auto itemIdx3 = itemIdx + 3;
        auto itemIdx4 = itemIdx + 4;
        auto itemIdx5 = itemIdx + 5;
        auto itemIdx6 = itemIdx + 6;
        auto itemIdx7 = itemIdx + 7;
        // compute the hash as  H(x) = AES(x) + x
        hashs[0] = hashs[0] ^ items[itemIdx0];
        hashs[1] = hashs[1] ^ items[itemIdx1];
        hashs[2] = hashs[2] ^ items[itemIdx2];
        hashs[3] = hashs[3] ^ items[itemIdx3];
        hashs[4] = hashs[4] ^ items[itemIdx4];
        hashs[5] = hashs[5] ^ items[itemIdx5];
        hashs[6] = hashs[6] ^ items[itemIdx6];
        hashs[7] = hashs[7] ^ items[itemIdx7];
        // Get the first bin that each of the items maps to
        auto bIdx00 = CuckooIndex<>::getHash(hashs[0], 0, numBins);
        auto bIdx10 = CuckooIndex<>::getHash(hashs[1], 0, numBins);
        auto bIdx20 = CuckooIndex<>::getHash(hashs[2], 0, numBins);
        auto bIdx30 = CuckooIndex<>::getHash(hashs[3], 0, numBins);
        auto bIdx40 = CuckooIndex<>::getHash(hashs[4], 0, numBins);
        auto bIdx50 = CuckooIndex<>::getHash(hashs[5], 0, numBins);
        auto bIdx60 = CuckooIndex<>::getHash(hashs[6], 0, numBins);
        auto bIdx70 = CuckooIndex<>::getHash(hashs[7], 0, numBins);
        // update the map with these bin indexes
        mItemToBinMap(itemIdx0, 0) = bIdx00;
        mItemToBinMap(itemIdx1, 0) = bIdx10;
        mItemToBinMap(itemIdx2, 0) = bIdx20;
        mItemToBinMap(itemIdx3, 0) = bIdx30;
        mItemToBinMap(itemIdx4, 0) = bIdx40;
        mItemToBinMap(itemIdx5, 0) = bIdx50;
        mItemToBinMap(itemIdx6, 0) = bIdx60;
        mItemToBinMap(itemIdx7, 0) = bIdx70;
        // get the second bin index
        auto bIdx01 = CuckooIndex<>::getHash(hashs[0], 1, numBins);
        auto bIdx11 = CuckooIndex<>::getHash(hashs[1], 1, numBins);
        auto bIdx21 = CuckooIndex<>::getHash(hashs[2], 1, numBins);
        auto bIdx31 = CuckooIndex<>::getHash(hashs[3], 1, numBins);
        auto bIdx41 = CuckooIndex<>::getHash(hashs[4], 1, numBins);
        auto bIdx51 = CuckooIndex<>::getHash(hashs[5], 1, numBins);
        auto bIdx61 = CuckooIndex<>::getHash(hashs[6], 1, numBins);
        auto bIdx71 = CuckooIndex<>::getHash(hashs[7], 1, numBins);
        // check if we get a collision with the first bin index
        u8 c01 = 1 & (bIdx00 == bIdx01);
        u8 c11 = 1 & (bIdx10 == bIdx11);
        u8 c21 = 1 & (bIdx20 == bIdx21);
        u8 c31 = 1 & (bIdx30 == bIdx31);
        u8 c41 = 1 & (bIdx40 == bIdx41);
        u8 c51 = 1 & (bIdx50 == bIdx51);
        u8 c61 = 1 & (bIdx60 == bIdx61);
        u8 c71 = 1 & (bIdx70 == bIdx71);
        // If we didnt get a collision, set the new bin index and otherwise set it to -1
        mItemToBinMap(itemIdx0, 1) = bIdx01 | (c01 * u64(-1));
        mItemToBinMap(itemIdx1, 1) = bIdx11 | (c11 * u64(-1));
        mItemToBinMap(itemIdx2, 1) = bIdx21 | (c21 * u64(-1));
        mItemToBinMap(itemIdx3, 1) = bIdx31 | (c31 * u64(-1));
        mItemToBinMap(itemIdx4, 1) = bIdx41 | (c41 * u64(-1));
        mItemToBinMap(itemIdx5, 1) = bIdx51 | (c51 * u64(-1));
        mItemToBinMap(itemIdx6, 1) = bIdx61 | (c61 * u64(-1));
        mItemToBinMap(itemIdx7, 1) = bIdx71 | (c71 * u64(-1));
        // repeat the process with the last hash function
        auto bIdx02 = CuckooIndex<>::getHash(hashs[0], 2, numBins);
        auto bIdx12 = CuckooIndex<>::getHash(hashs[1], 2, numBins);
        auto bIdx22 = CuckooIndex<>::getHash(hashs[2], 2, numBins);
        auto bIdx32 = CuckooIndex<>::getHash(hashs[3], 2, numBins);
        auto bIdx42 = CuckooIndex<>::getHash(hashs[4], 2, numBins);
        auto bIdx52 = CuckooIndex<>::getHash(hashs[5], 2, numBins);
        auto bIdx62 = CuckooIndex<>::getHash(hashs[6], 2, numBins);
        auto bIdx72 = CuckooIndex<>::getHash(hashs[7], 2, numBins);
        u8 c02 = 1 & (bIdx00 == bIdx02 || bIdx01 == bIdx02);
        u8 c12 = 1 & (bIdx10 == bIdx12 || bIdx11 == bIdx12);
        u8 c22 = 1 & (bIdx20 == bIdx22 || bIdx21 == bIdx22);
        u8 c32 = 1 & (bIdx30 == bIdx32 || bIdx31 == bIdx32);
        u8 c42 = 1 & (bIdx40 == bIdx42 || bIdx41 == bIdx42);
        u8 c52 = 1 & (bIdx50 == bIdx52 || bIdx51 == bIdx52);
        u8 c62 = 1 & (bIdx60 == bIdx62 || bIdx61 == bIdx62);
        u8 c72 = 1 & (bIdx70 == bIdx72 || bIdx71 == bIdx72);
        mItemToBinMap(itemIdx0, 2) = bIdx02 | (c02 * u64(-1));
        mItemToBinMap(itemIdx1, 2) = bIdx12 | (c12 * u64(-1));
        mItemToBinMap(itemIdx2, 2) = bIdx22 | (c22 * u64(-1));
        mItemToBinMap(itemIdx3, 2) = bIdx32 | (c32 * u64(-1));
        mItemToBinMap(itemIdx4, 2) = bIdx42 | (c42 * u64(-1));
        mItemToBinMap(itemIdx5, 2) = bIdx52 | (c52 * u64(-1));
        mItemToBinMap(itemIdx6, 2) = bIdx62 | (c62 * u64(-1));
        mItemToBinMap(itemIdx7, 2) = bIdx72 | (c72 * u64(-1));
    }
    // in case the input does not divide evenly by 8, handle the last few items.
    hasher.ecbEncBlocks(items.data() + itemIdx, remSteps, hashs.data());
    for (u64 i = 0; i < remSteps; ++i, ++itemIdx)
    {
        hashs[i] = hashs[i] ^ items[itemIdx];
        std::vector<u64> bIdxs(mNumHashFunctions);
        for (u64 h = 0; h < mNumHashFunctions; ++h)
        {
            auto bIdx = CuckooIndex<>::getHash(hashs[i], (u8)h, numBins);
            bool collision = false;
            bIdxs[h] = bIdx;
            for (u64 hh = 0; hh < h; ++hh)
                collision |= (bIdxs[hh] == bIdx);
            u8 c = ((u8)collision & 1);
            mItemToBinMap(itemIdx, h) = bIdx | c * u64(-1);
        }
    }
}

void server_hash_step(std::vector<oc::block> &inputs, CuckooIndex<NotThreadSafe> &mIndex) {    
    
    mIndex.init(CuckooParam{ hash_stash_size, hash_e, hash_hash_num, inputs.size() });
    
    mIndex.insert(inputs, commSeed);
}

void client_psi_step(std::vector<oc::block> &psiInputs, std::vector<oc::u64> &mIntersection) {

    recvPSIs.sendInput(psiInputs, osu_chl);

    mIntersection = recvPSIs.mIntersection;
}

void server_psi_step(std::vector<oc::block> &psiInputs) {

    sendPSIs.sendInput(psiInputs, osu_chl);
}