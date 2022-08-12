int hash_stash_size = 0;
double hash_e = 1.27;
int hash_hash_num = 3;
int stat_sec_param = 40;

int payload_bit = 7;
uint64_t payload_mask = (uint64_t(1) << payload_bit) - 1;

int ezpc_port = 1;
int osu_port = 101;

int party, port = 32000;
int num_threads = 1;
std::string address = "127.0.0.1";

int clientSize = 10000, serverSize = 1000000;