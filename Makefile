CXX = g++

CXXFLAGS_COMMON = -std=c++17 -pthread -Iheaders -O3 -fopenmp
CXXFLAGS_SERVER = $(CXXFLAGS_COMMON) -mavx2
CXXFLAGS_CLIENT = $(CXXFLAGS_COMMON)

SERVER_SRC = main_server.cpp \
             sources/network/server.cpp \
             sources/network/net_utils.cpp \
             sources/models/Model.cpp \
             sources/models/Architecture.cpp \
             sources/models/Layer.cpp \
             sources/network/connection.cpp \
             sources/network/FLcoordinator.cpp \
             sources/network/sharedState.cpp \
             sources/serializer/*.cpp \
             sources/security/AuthManager.cpp \
             sources/security/envUtils.cpp \
             sources/security/SHA256.cpp \
             sources/protocol/*.cpp \
             sources/FedAvg/*.cpp \
             sources/logger/logger.cpp \
             sources/jsonManager/jsonManager.cpp\
             sources/jsonManager/jsonServerManager.cpp \
             sources/weightsUtils/weightsUtils.cpp

CLIENT_SRC = main_client.cpp \
             sources/network/client.cpp \
             sources/network/net_utils.cpp \
             sources/models/Model.cpp \
             sources/models/Architecture.cpp \
             sources/models/Layer.cpp \
             sources/serializer/serializer.cpp \
             sources/network/connection.cpp \
             sources/protocol/*.cpp\
             sources/logger/logger.cpp \
             sources/jsonManager/jsonManager.cpp\
             sources/jsonManager/jsonClientManager.cpp 

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