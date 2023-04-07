# Predicate Private Set Intersection With Linear Complexity

### Introduction
This directory contains the code for "Predicate Private Set Intersection With Linear Complexity".

### Repo Directory Description
- `include/` Contains implementation of PPSI's batch millionaires protocol.
- `frontend` Contains implementation of PPSI.
- `patch/` Patches applied to the dependent libraries.
- `scripts/` Helper scripts used to build the programs in this repo.

### Requirements

* openssl 
* c++ compiler (>= 8.0 for the better performance on AVX512)
* cmake >= 3.18
* git
* make

### Building Dependencies
* Run `bash scripts/build-deps.sh` which will build the following dependencies
    * [emp-tool](https://github.com/emp-toolkit/emp-tool) We follow the implementation in SCI that using emp-tool for network io and pseudo random generator.
	* [emp-ot](https://github.com/emp-toolkit/emp-ot) We use Ferret in emp-ot as our VOLE-style OT.
	* [Eigen](https://github.com/libigl/eigen) Needed by Cheetah.
	* [SEAL](https://github.com/microsoft/SEAL) Needed by Cheetah.
	* [zstd](https://github.com/facebook/zstd) Needed by Cheetah.
	* [hexl](https://github.com/intel/hexl/tree/1.2.2) Needed by Cheetah.
	* [libPSI](https://github.com/osu-crypto/libPSI) We use KKRT for PPSI.

* The generated objects are placed in the `build/deps/` folder.
* Build has passed on the following setting
  * Ubuntu 22.04 with gcc 9.4.0 cmake 3.25.1
  
### Building PPSI frontend

* Run `bash scripts/build.sh` which will build excutable file `frontend` in the `build/bin` folder
* VOLE-style OT and IKNP-style OT is optional by [USE_CHEETAH](https://github.com/cmZoO/PPSI/blob/main/CMakeLists.txt#L10).

### Running the Code
#### Parameters:
```
r:       Role of party: Client = 1; Server = 2 
ip:      IP Address of server (Server)
cs:      Input size of Client
ss:      Input size of Server
```
#### Examples:
```
$ build/bin/frontend r=1 cs=1000000 ss=1000000
$ build/bin/frontend r=2 cs=1000000 ss=1000000

$ build/bin/frontend r=1 cs=10000 ss=1000000
$ build/bin/frontend r=2 cs=10000 ss=1000000
```
### Mimic an WAN setting within LAN on Linux

* To use the throttle script under [scripts/throttle.sh](scripts/throttle.sh) to limit the network speed and ping latency (require `sudo`)
* For example, run `sudo scripts/throttle.sh wan` on a Linux OS which will limit the local-loop interface to about 400Mbps bandwidth and 40ms ping latency.
  You can check the ping latency by just `ping 127.0.0.1`. The bandwidth can be check using extra `iperf` command.

### Help
For any questions on building or running the library, please contact yufeng at yyfeng5834@gmail.com.
