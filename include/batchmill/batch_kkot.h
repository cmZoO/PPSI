#pragma once

#include "OT/np.h"
#include "OT/ot.h"
#include "OT/kkot.h"
#include "utils/emp-tool.h"

using namespace sci;

template <typename IO> class BatchKKOT {
public:
    KKOT<sci::NetIO>* kkot;
    IO* io = nullptr;
    sci::CRH crh;
    BatchKKOT(IO* io, KKOT<sci::NetIO>* kkot) {
        this->io = io;
        this->kkot = kkot;
    }

    ~BatchKKOT() {
    }

    void send_batch_kkot_post(uint8_t** data, uint64_t* N, int ot_num, 
        uint64_t batch_length, int bit_length) {
        // 发送的每条消息所需要的128位hash个数，对于每个ot，需要N[i]条消息
        uint64_t numHashes = ceil_val(bit_length * batch_length, 128);
        // 每个ot的消息hash
        block128** hashArr = new block128 * [N[0]];
        for (int i = 0; i < N[0]; i++) {
            hashArr[i] = new block128[numHashes + 1];
        }
        //block128 hashArr[N[0]][numHashes + 1];
        // 使用hash mask过的一条消息
        block128 * maskArr = new block128[numHashes + 1];
        //block128 maskArr[numHashes + 1];
        // 发送过去的mask数据，每个ot发送最多N[0]条，没条最多numHashes长
        block128* dataToBeSent =
            new block128[ot_num * N[0] * numHashes];
        // 发送过去的总的byte数，每条消息末端不足128位会补齐位128位
        uint64_t dataToBeSentByte = 0;
        // 每条消息使用的N[0]个key
        block256* key = new (std::align_val_t(32)) block256[N[0]];
        // 256位的key经过随机预言机后得到的初始hash
        block128* key_hash = new block128[N[0]];
        // 循环每个ot进行处理
        for (int i = 0; i < ot_num; i++) {
            // 对于当次ot，循环N[i]进行每条消息的处理
            for (int k = 0; k < N[i]; k++) {
                // 获取第i个ot的第k条消息的初始key
                key[k] = xorBlocks(kkot->qT[i], kkot->c_AND_s[k]);
            }
            // 获得第i次ot的N[i]的H后的key
            CCRF(key_hash, key, N[i]);
            // 对于每条消息
            for (int k = 0; k < N[i]; k++) {
                // 将初始key拓展成消息的长度
                for (int j = 0; j < numHashes; j++) {
                    hashArr[k][j] = xorBlocks(key_hash[k], toBlock(j));
                }
                // 对拓展后的key进行hash，获得padding
                crh.Hn(hashArr[k], hashArr[k], numHashes);
            }
            // 处理每条消息
            for (int k = 0; k < N[i]; k++) {
                // 处理每条消息的每一个子数据(每条消息都是一个批，由多条子数据组成)
                for (uint64_t j = 0; j < batch_length; j++) {
                    // 获取改子数据对应的随机padding
                    uint64_t randVal =
                        readFromPackedArr((uint8_t*)hashArr[k], 16 * numHashes,
                            j * bit_length, bit_length);
                    // 发送的数据加密
                    uint64_t maskedVal = data[i * batch_length + j][k] ^ randVal;
                    // 将加密后的数据写入已mask消息中，准备发送
                    writeToPackedArr((uint8_t*)maskArr, 16 * numHashes,
                        j * bit_length, bit_length, maskedVal);
                }
                // 第k条消息的长度，padding，原消息，padding消息是一致的
                uint64_t bytesToBeSent = ceil_val(bit_length * batch_length,
                    8);
                // 将消息加入到发送buffer中
                memcpy(((uint8_t*)dataToBeSent) + dataToBeSentByte, maskArr,
                    bytesToBeSent);
                // buffer长度累加
                dataToBeSentByte += bytesToBeSent;
            }
        }
        // 发送消息
        io->send_data(dataToBeSent, dataToBeSentByte);
        io->flush();

        for (int i = 0; i < N[0]; i++) {
            delete[] hashArr[i];
        }
        delete[] hashArr;
        delete[] maskArr;
        delete[] dataToBeSent;
        delete[] key;
        delete[] key_hash;
    }

    void send_batch_kkot(uint8_t** data, uint64_t* N, int ot_num, uint64_t batch_length, int bit_length) {
        if (ot_num < 1)
            return;
        assert(N[0] <= kkot->lambda && N[0] >= 2);
        kkot->N = N[0];
        if ((kkot->max_N < N[0]) && kkot->setup == true) {
            kkot->precompute_masks();
        }
        kkot->send_pre(ot_num);
        send_batch_kkot_post(data, N, ot_num, batch_length, bit_length);
    }

    void recv_batch_kkot_post(uint8_t* res, uint64_t* N, uint8_t* choice, int ot_num, uint64_t batch_length, int bit_length) {
        // 发送的每条消息所需要的128位hash个数，对于每个ot，需要N[i]条消息
        uint64_t numHashes = ceil_val(bit_length * batch_length, 128);
        // 接收的mask数据，每个ot接收最多N[0]条，每条最多numHashes长
        block128* dataToBeRecv =
            new block128[ot_num * N[0] * numHashes];
        // 发送过去的总的byte数，每条消息末端不足128位会补齐位128位
        uint64_t dataToBeRecvByte = 0;
        // 每条ot使用的恢复数据的key
        block128* key = new block128[ot_num];
        // 使用hash mask过的一条消息
        block128 * maskArr = new block128[numHashes + 1];
        //block128 maskArr[numHashes + 1];
        // 一条消息的hash
        block128 * hashArr = new block128[numHashes + 1];
        //block128 hashArr[numHashes + 1];
        // 每条消息的长度，padding，原消息，padding消息是一致的
        uint64_t eachMesgBytesToBeRecv = ceil_val(bit_length * batch_length,
            8);
        // 循环每个ot进行处理
        for (int i = 0; i < ot_num; i++) {
            // buffer长度累加
            dataToBeRecvByte += eachMesgBytesToBeRecv * N[i];
        }
        // 接收所有发送过来的消息
        io->recv_data(dataToBeRecv, dataToBeRecvByte);
        // 获取每个ot恢复数据所使用的key
        CCRF(key, kkot->tT, ot_num);
        // 当前OT的head指针
        uint8_t* dataHead = (uint8_t*)dataToBeRecv;
        // 解密每个ot的数据
        for (int i = 0; i < ot_num; i++) {
            // 取出挑选bit对应的mask数据
            memcpy(maskArr, dataHead + choice[i] * eachMesgBytesToBeRecv,
                eachMesgBytesToBeRecv);
            dataHead = dataHead + N[i] * eachMesgBytesToBeRecv;
            // 将初始key拓展成消息的长度
            for (int j = 0; j < numHashes; j++) {
                hashArr[j] = xorBlocks(key[i], toBlock(j));
            }
            // 对拓展后的key进行hash，获得padding
            crh.Hn(hashArr, hashArr, numHashes);
            // 对消息解密
            for (int j = 0; j < numHashes; j++) {
                maskArr[j] = xorBlocks(maskArr[j], hashArr[j]);
            }
            // 将消息中的每个子数据提取出来
            for (uint64_t j = 0; j < batch_length; j++) {
                // 提取消息
                uint64_t val =
                    readFromPackedArr((uint8_t*)maskArr, 16 * numHashes,
                        j * bit_length, bit_length);
                res[i * batch_length + j] = val & (all1Mask(bit_length));
            }
        }

        delete[] dataToBeRecv;
        delete[] key;
        delete[] maskArr;
        delete[] hashArr;
    }

    void recv_batch_kkot(uint8_t* res, uint64_t* N, uint8_t* choice, int ot_num, uint64_t batch_length, int bit_length) {
        if (ot_num < 1)
            return;
        assert(N[0] <= kkot->lambda && N[0] >= 2);
        kkot->N = N[0];
        kkot->recv_pre(choice, ot_num);
        recv_batch_kkot_post(res, N, choice, ot_num, batch_length, bit_length);
    }
};