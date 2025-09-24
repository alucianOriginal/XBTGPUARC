CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -DCL_TARGET_OPENCL_VERSION=300
LDFLAGS = -lOpenCL -lboost_system -lboost_json -lpthread

SRC = main.cpp miner_loop.cpp stratum_client.cpp
OUT = xbtgpuarc

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)
