CXX = g++

CXXFLAGS_COMMON = -std=c++17 -pthread -Icode/headers -O3 -fopenmp
CXXFLAGS_SERVER = $(CXXFLAGS_COMMON) -mavx2
CXXFLAGS_CLIENT = $(CXXFLAGS_COMMON)

SERVER_SRC = main_server.cpp \
             code/sources/network/server.cpp \
             code/sources/network/net_utils.cpp \
             code/sources/network/serverConsoleManager.cpp \
             code/sources/models/Model.cpp \
             code/sources/models/Architecture.cpp \
             code/sources/models/Layer.cpp \
             code/sources/network/connection.cpp \
             code/sources/serializer/*.cpp \
             code/sources/security/AuthManager.cpp \
             code/sources/security/envUtils.cpp \
             code/sources/security/SHA256.cpp \
             code/sources/security/backdoorDefense.cpp \
             code/sources/protocol/*.cpp \
             code/sources/federated/*.cpp \
             code/sources/logger/logger.cpp \
             code/sources/jsonManager/jsonManager.cpp\
             code/sources/jsonManager/jsonServerManager.cpp \
             code/sources/weightsUtils/weightsUtils.cpp \
             code/sources/evaluation/evaluationRunner.cpp \
             code/sources/datasetutils/datasetSplitter.cpp

CLIENT_SRC = main_client.cpp \
             code/sources/network/client.cpp \
             code/sources/network/net_utils.cpp \
             code/sources/models/Model.cpp \
             code/sources/models/Architecture.cpp \
             code/sources/models/Layer.cpp \
             code/sources/serializer/serializer.cpp \
             code/sources/network/connection.cpp \
             code/sources/protocol/protocol.cpp\
             code/sources/protocol/clientProtocol.cpp\
             code/sources/logger/logger.cpp \
             code/sources/jsonManager/jsonManager.cpp\
             code/sources/jsonManager/jsonClientManager.cpp \
             code/sources/localTraining/localTraining.cpp \
             code/sources/datasetutils/datasetutils.cpp

SERVER_BIN = server_app
CLIENT_BIN = client_app

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN):
	$(CXX) $(CXXFLAGS_SERVER) $(SERVER_SRC) -o $(SERVER_BIN)

$(CLIENT_BIN):
	$(CXX) $(CXXFLAGS_CLIENT) $(CLIENT_SRC) -o $(CLIENT_BIN)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)

run_server: $(SERVER_BIN)
	./$(SERVER_BIN) $(CONFIG)

run_client: $(CLIENT_BIN)
	./$(CLIENT_BIN) $(CONFIG)