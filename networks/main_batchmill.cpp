// aemo-batch-mill.cpp: 目标的源文件。
//

#include "Math/math-functions.h"
#include "batchmill/batch_millionaire.h"
#include <thread>

using namespace sci;
using namespace std;

#define MAX_THREADS 8

int party, port = 32000;
int num_threads = 1;
string address = "127.0.0.1";

int dim = 1 << 10;
int bw_x = 7;

sci::NetIO* ioArr[MAX_THREADS];
sci::KKOT<sci::NetIO>* kkotArr[MAX_THREADS];
sci::OTPack<sci::NetIO>* otpackArr[MAX_THREADS];

void batch_cmp_thread(int tid, uint64_t cmp, uint64_t* data, uint8_t* res, int num_ops) {
    BatchMillionaireProtocol<sci::NetIO>* batch_mill;

    batch_mill = new BatchMillionaireProtocol(party, ioArr[tid], kkotArr[tid], otpackArr[tid]);

    batch_mill->batch_compare(res, data, cmp, num_ops, bw_x);

    delete batch_mill;
}

int main(int argc, char** argv) {
    /************* Argument Parsing  ************/
    /********************************************/
    ArgMapping amap;
    amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
    amap.arg("p", port, "Port Number");
    amap.arg("N", dim, "Number of Compare operations");
    amap.arg("nt", num_threads, "Number of threads");
    amap.arg("ip", address, "IP Address of server (ALICE)");

    amap.parse(argc, argv); 

    /********** Setup IO and Base OTs ***********/
    /********************************************/
    for (int i = 0; i < num_threads; i++) {
        ioArr[i] = new sci::NetIO(party == 1 ? nullptr : address.c_str(), port + i);
        kkotArr[i] = new sci::KKOT<sci::NetIO>(ioArr[i]);
        otpackArr[i] = new OTPack<sci::NetIO>(ioArr[i], party);
        if (party == sci::ALICE) {
            kkotArr[i]->setup_send();
        }
        else {
            kkotArr[i]->setup_recv();
        }
    }
    std::cout << "All Base OTs Done" << std::endl;

    /************ Generate Test Data ************/
    /********************************************/
    sci::PRG128 prg;

    uint64_t cmp;
    uint64_t* data = new uint64_t[dim];

    prg.random_data(&cmp, sizeof(uint64_t));
    prg.random_data(data, dim * sizeof(uint64_t));

    /************** Fork Threads ****************/
    /********************************************/
    uint64_t total_comm = 0;
    uint64_t thread_comm[num_threads];
    for (int i = 0; i < num_threads; i++) {
        thread_comm[i] = ioArr[i]->counter;
    }
    auto start = high_resolution_clock::now();

    uint8_t* res = new uint8_t[dim];
    std::thread cmp_threads[num_threads];
    int chunk_size = dim / num_threads;
    for (int i = 0; i < num_threads; ++i) {
        int offset = i * chunk_size;
        int lnum_ops;
        if (i == (num_threads - 1)) {
            lnum_ops = dim - offset;
        }
        else {
            lnum_ops = chunk_size;
        }
        cmp_threads[i] =
            std::thread(batch_cmp_thread, i, cmp, data + offset, res + offset, lnum_ops);
    }
    for (int i = 0; i < num_threads; ++i) {
        cmp_threads[i].join();
    }
    long long t = std::chrono::duration_cast<std::chrono::microseconds>(high_resolution_clock::now() - start).count();

    for (int i = 0; i < num_threads; i++) {
        thread_comm[i] = ioArr[i]->counter - thread_comm[i];
        total_comm += thread_comm[i];
    }

    /************** Verification ****************/
    /********************************************/
    if (party == sci::ALICE) {
        uint8_t* res0 = new uint8_t[dim];
        uint64_t cmp0;
        ioArr[0]->recv_data(res0, dim * sizeof(uint8_t));
        ioArr[0]->recv_data(&cmp0, sizeof(uint64_t));

        for (int i = 0; i < dim; i++) {
            uint64_t t_res = unsigned_val(res[i] + res0[i], 1);
            cmp0 = unsigned_val(cmp0, bw_x);
            data[i] = unsigned_val(data[i], bw_x);
            uint64_t expectedRes = data[i] > cmp0;
            assert(t_res == expectedRes);
        }

        cout << "Batch Cmp Tests Passed" << endl;
    }
    else { // party == BOB
        ioArr[0]->send_data(res, dim * sizeof(uint8_t));
        ioArr[0]->send_data(&cmp, sizeof(uint64_t));
    }

    /**** Process & Write Benchmarking Data *****/
    /********************************************/
    cout << "Number of Batch Cmp/s:\t" << (double(dim) / t) * 1e6 << std::endl;
    cout << "Batch Cmp Pre Time\t" << pre_time / (1000.0) << " ms" << endl;
    cout << "Batch Cmp Pre Bytes Sent\t" << pre_comm << " bytes" << endl;
    cout << "Batch Cmp Online Time\t" << (t - pre_time) / (1000.0) << " ms" << endl;
    cout << "Batch Cmp Online Bytes Sent\t" << (total_comm - pre_comm) << " bytes" << endl;
    cout << "Batch Cmp Time\t" << t / (1000.0) << " ms" << endl;
    cout << "Batch Cmp Bytes Sent\t" << total_comm << " bytes" << endl;

    /******************* Cleanup ****************/
    /********************************************/
    for (int i = 0; i < num_threads; i++) {
        delete ioArr[i];
        delete otpackArr[i];
    }

	return 0;
}
