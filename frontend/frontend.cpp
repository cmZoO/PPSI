#include <iostream>
#include <string>

#include "define.h"
#include "ezpc.h"
#include "osu.h"
#include "utils/ArgMapping/ArgMapping.h"

using namespace std;

void check_results(std::vector<u64> &mIntersection, uint64_t client_payload) {
    std::vector<uint64_t> server_payloads(serverSize);
    io->recv_data(server_payloads.data(), serverSize * sizeof(uint64_t));
    int index = 0;
    std::sort(mIntersection.begin(), mIntersection.end());
    for (int i = 0; i < std::min(clientSize, serverSize) / 2; i++) {
        if (client_payload < server_payloads[i]) {
            assert(mIntersection[index++] / 3 == i);
        }
    }
    assert(index == mIntersection.size());
    std::cout << "intersection size: " << mIntersection.size() << std::endl;
}

void Client() {
    std::vector<oc::block> inputs(clientSize);
    for (int i = 0; i < clientSize / 2; i++) {
        inputs[i] = commPrng.get<oc::block>();
    }
    oc::PRNG prng1(oc::toBlock(456));
    for (int i = clientSize / 2; i < clientSize; i++) {
        inputs[i] = prng1.get<oc::block>();
    }
    uint64_t payload = prng1.get<uint64_t>() % payload_mask;

    ezpc_init();
    osu_init(prng1);

    auto start = high_resolution_clock::now();
    uint64_t ezpc_cient_comm = io->counter;

    CuckooParam serverParam = CuckooParam{ hash_stash_size, hash_e, hash_hash_num, serverSize };
    oc::Matrix<u64> mInputToBinMap(clientSize, hash_hash_num);
    client_hash_step(inputs, mInputToBinMap, serverParam.numBins());
    
    uint8_t *payload_cmp_res = new uint8_t[serverParam.numBins()];
    client_payload_cmp_step(payload, payload_cmp_res, serverParam.numBins());
    
    std::vector<oc::block> psiInputs(clientSize * hash_hash_num);
    client_pred_mask_step(psiInputs, inputs, payload_cmp_res, mInputToBinMap, serverParam.numBins());
    
    std::vector<u64> mIntersection;
    client_psi_step(psiInputs, mIntersection);

    long long t = std::chrono::duration_cast<std::chrono::microseconds>(high_resolution_clock::now() - start).count();
    std::cout << "time cost(ms)  : " << t / (1000.0) << std::endl;
    ezpc_cient_comm = io->counter - ezpc_cient_comm;
    uint64_t ezpc_server_comm;
    io->recv_data(&ezpc_server_comm, sizeof(uint64_t));
    u64 osu_comm = osu_chl.getTotalDataSent() + osu_chl.getTotalDataRecv();
    std::cout << "comm cost(byte): " << ezpc_cient_comm + ezpc_server_comm + osu_comm << std::endl;

    check_results(mIntersection, payload);

    delete[] payload_cmp_res;
    ezpc_end();
}

void Server() {
    std::vector<oc::block> inputs(serverSize);
    for (int i = 0; i < serverSize / 2; i++) {
        inputs[i] = commPrng.get<oc::block>();
    }
    oc::PRNG prng2(oc::toBlock(789));
    for (int i = serverSize / 2; i < serverSize; i++) {
        inputs[i] = prng2.get<oc::block>();
    }
    std::vector<uint64_t> payloads(serverSize);
    for (int i = 0; i < serverSize; i++) {
        payloads[i] = prng2.get<uint64_t>() % payload_mask;
    }

    ezpc_init();
    osu_init(prng2);

    uint64_t ezpc_server_comm = io->counter;

    CuckooIndex<NotThreadSafe> mIndex;
    server_hash_step(inputs, mIndex);

    uint8_t *payload_cmp_res = new uint8_t[mIndex.mBins.size()];
    server_payload_cmp_step(payloads, payload_cmp_res, mIndex);
    
    std::vector<oc::block> psiInputs(serverSize);
    server_pred_mask_step(psiInputs, inputs, payload_cmp_res, mIndex);
    
    server_psi_step(psiInputs);

    ezpc_server_comm = io->counter - ezpc_server_comm;
    io->send_data(&ezpc_server_comm, sizeof(uint64_t));
    io->send_data(payloads.data(), serverSize * sizeof(uint64_t));

    delete[] payload_cmp_res;
    ezpc_end();
}   

int main(int argc, char** argv) {
    /************* Argument Parsing  ************/
    /********************************************/
    ArgMapping amap;
    amap.arg("r", party, "Role of party: Client = 1; Server = 2");
    amap.arg("p", port, "Port Number");
    amap.arg("ip", address, "IP Address of server (Server)");
    amap.arg("cs", clientSize, "input size of Client");
    amap.arg("ss", serverSize, "input size of Server");

    amap.parse(argc, argv); 

    if (party == 1) {
        Client();
    } else {
        Server();
    }

	return 0;
}
