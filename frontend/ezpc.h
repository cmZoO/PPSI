#include "Math/math-functions.h"
#include "batchmill/batch_millionaire.h"
#include "cryptoTools/Common/CuckooIndex.h"
#include "cryptoTools/Common/block.h"

using namespace sci;

sci::NetIO* io;
sci::KKOT<sci::NetIO>* kkot;
sci::IKNP<sci::NetIO>* iknp;
sci::OTPack<sci::NetIO>* otpack;

/**
 * @brief in ezpc.h
 *  Client act as BOB   & OT receviver
 *  Server act as ALICE & OT sender
 */

void ezpc_init() {
    io = new sci::NetIO(party == sci::BOB ? nullptr : address.c_str(), port + ezpc_port);
    kkot = new sci::KKOT<sci::NetIO>(io);
    iknp = new sci::IKNP<sci::NetIO>(io);
    otpack = new OTPack<sci::NetIO>(io, 3 - party);
    if (party == sci::ALICE) {
        kkot->setup_recv();
        iknp->setup_recv();
    } else {
        kkot->setup_send();
        iknp->setup_send();
    }
    io->flush();
    std::cout << "All Base OTs Done" << std::endl;
}

void ezpc_end() {
    delete io;
    delete kkot;
    delete otpack;
}

void client_payload_cmp_step(uint64_t payload, uint8_t* payload_cmp_res, uint64_t numBins) {
    
    BatchMillionaireProtocol<sci::NetIO>* batch_mill = 
            new BatchMillionaireProtocol(3 - party, io, kkot, otpack);
    batch_mill->batch_compare(payload_cmp_res, nullptr, payload, numBins, payload_bit);
    
    delete batch_mill;
}

void server_payload_cmp_step(std::vector<uint64_t> &payloads, uint8_t* &payload_cmp_res, oc::CuckooIndex<oc::NotThreadSafe> &mIndex) {
    uint64_t *data = new uint64_t[mIndex.mBins.size()];
    sci::PRG128 prg;
    for (int i = 0; i < mIndex.mBins.size(); i++) {
        auto& bin = mIndex.mBins[i];
        if (bin.isEmpty() == false) {
            data[i] = payloads[bin.idx()];
        } else {
            prg.random_data(data + i, sizeof(uint64_t));
            data[i] %= payload_mask;
        }
    }

    BatchMillionaireProtocol<sci::NetIO>* batch_mill = 
            new BatchMillionaireProtocol(3 - party, io, kkot, otpack);
    batch_mill->batch_compare(payload_cmp_res, data, 0, mIndex.mBins.size(), payload_bit);

    delete batch_mill;
    delete[] data;
}

void client_pred_mask_step(
    std::vector<oc::block> &psiInputs, 
    std::vector<oc::block> &inputs, 
    uint8_t *payload_cmp_res, 
    oc::Matrix<oc::u64> &mInputToBinMap,
    uint64_t numBins) {

    sci::block128 *mask = new sci::block128[numBins];
#if USE_CHEETAH
    std::cout << "use silent rot" << std::endl;
    otpack->iknp_straight->recv_ot_rm_cc(mask, (bool *)payload_cmp_res, numBins);
#else
    std::cout << "use iknp rot" << std::endl;
    iknp->recv_rot(mask, (bool *)payload_cmp_res, numBins);
#endif
    io->flush();
    sci::PRG128 prg;
    for (int i = 0; i < mInputToBinMap.rows(); i++) {
        for (int j = 0; j < mInputToBinMap.cols(); j++) {
            auto& bIdx = mInputToBinMap(i, j);
            oc::block item = inputs[i];
            if (bIdx == -1) {
                prg.random_block(&item.mData);
            } else {
                item.mData = _mm_xor_si128(item.mData, mask[bIdx]);
            }
            psiInputs[i * mInputToBinMap.cols() + j] = item;
        }
    }

    delete[] mask;
}

void server_pred_mask_step(
    std::vector<oc::block> &psiInputs, 
    std::vector<oc::block> &inputs, 
    uint8_t *payload_cmp_res, 
    oc::CuckooIndex<oc::NotThreadSafe> &mIndex) {
    uint64_t numBins = mIndex.mBins.size();
    sci::block128 *data0 = new sci::block128[numBins];
    sci::block128 *data1 = new sci::block128[numBins];
#if USE_CHEETAH
    std::cout << "use silent rot" << std::endl;
    otpack->iknp_straight->send_ot_rm_cc(data0, data1, numBins);
#else
    std::cout << "use iknp rot" << std::endl;
    iknp->send_rot(data0, data1, numBins);
#endif
    io->flush();
    for (int i = 0; i < numBins; i++) {
        if (!payload_cmp_res[i]) {
            data0[i] = data1[i];
        }
    }
    sci::block128 *mask = data0;
    delete[] data1;

    int index = 0;
    for (int i = 0; i < numBins; i++) {
        auto& bin = mIndex.mBins[i];
        if (bin.isEmpty() == false) {
            oc::block item = inputs[bin.idx()];
            item.mData = _mm_xor_si128(item.mData, mask[i]);
            psiInputs[index++] = item;
        }
    }

    delete[] mask;
}

