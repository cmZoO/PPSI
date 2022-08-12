#include "Math/math-functions.h"
#include "batchmill/batch_kkot.h"
#include "batchmill/batch_millionaire.h"

using namespace sci;
using namespace std;

#define MAX_THREADS 8

int party, port = 32000;
int num_threads = 1;
string address = "127.0.0.1";

sci::NetIO* ioArr;
sci::KKOT<sci::NetIO>* kkotArr;
BatchKKOT<sci::NetIO>* batchKKOT;

int main(int argc, char** argv) {
    /************* Argument Parsing  ************/
    /********************************************/
    ArgMapping amap;
    amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
    amap.arg("p", port, "Port Number");
    amap.arg("nt", num_threads, "Number of threads");
    amap.arg("ip", address, "IP Address of server (ALICE)");

    amap.parse(argc, argv);

    /********** Setup IO and Base OTs ***********/
    /********************************************/
    ioArr = new sci::NetIO(party == 1 ? nullptr : address.c_str(), port);
    kkotArr = new sci::KKOT<sci::NetIO>(ioArr);
    batchKKOT = new BatchKKOT(ioArr, kkotArr);
    std::cout << "All Base OTs Done" << std::endl;

    /************ Generate Test Data ************/
    /********************************************/
    sci::PRG128 prg;

    int batch_length = 1000000;
    int bit_length = 3;
    int ot_num = 8;
    int max_N = 17;
    uint64_t mask = (1 << bit_length) - 1;

    uint64_t* N = new uint64_t[ot_num];
    for (int i = 0; i < ot_num - 1; i++) {
        N[i] = max_N;
    }
    prg.random_data(N + ot_num - 1, sizeof(uint64_t));
    N[ot_num - 1] = N[ot_num - 1] % max_N;
    N[ot_num - 1] = max(N[ot_num - 1], (uint64_t)2);
    if (party == sci::ALICE) {
        ioArr->recv_data(N + ot_num - 1, sizeof(uint64_t));
    }
    else {
        ioArr->send_data(N + ot_num - 1, sizeof(uint64_t));
    }

    uint8_t** data = new uint8_t*[batch_length * ot_num];
    for (int i = 0; i < ot_num * batch_length; i++) {
        data[i] = new uint8_t[N[i / batch_length]];
        prg.random_data(data[i], N[i / batch_length] * sizeof(uint8_t));
    }
    uint8_t* res = new uint8_t [batch_length * ot_num];
    uint8_t* choice = new uint8_t[ot_num];
    prg.random_data(choice, ot_num * sizeof(uint8_t));
    for (int i = 0; i < ot_num; i++) {
        choice[i] %= N[i];
    }

    uint64_t total_comm = ioArr->counter;

    if (party == sci::ALICE) {
        batchKKOT->send_batch_kkot(data, N, ot_num, batch_length, bit_length);
        ioArr->recv_data(choice, sizeof(uint8_t) * ot_num);
        total_comm = ioArr->counter - total_comm;
        ioArr->recv_data(res, sizeof(uint8_t) * ot_num * batch_length);
        for (int j = 0; j < batch_length; j++) {
            for (int i = 0; i < ot_num; i++) {
                int a = data[i * batch_length + j][choice[i]] & mask;
                int b = res[i * batch_length +j];
                assert(a == b);
            }
        }
        cout << "success" << endl;
    }
    else {
        batchKKOT->recv_batch_kkot(res, N, choice, ot_num, batch_length, bit_length);
        total_comm = ioArr->counter - total_comm;
        ioArr->send_data(choice, sizeof(uint8_t) * ot_num);
        ioArr->send_data(res, sizeof(uint8_t) * ot_num *batch_length);
    }
    cout << "Batch KKOT Bytes Sent\t" << total_comm << " bytes" << endl;
}