CXX = g++
CXXFLAGS = -std=c++17 -pthread -Iheaders -O3 -mavx2 -fopenmp

SERVER_SRC = main_server.cpp \
             sources/network/server.cpp \
             sources/network/net_utils.cpp \
             sources/models/Model.cpp \
             sources/models/Architecture.cpp \
             sources/models/Layer.cpp \
             sources/network/connection.cpp \
             sources/network/sharedState.cpp \
             sources/serializer/*.cpp \
             sources/security/AuthManager.cpp \
             sources/security/envUtils.cpp \
             sources/security/SHA256.cpp \
             sources/protocol/*.cpp \
             sources/FedAvg/*.cpp

             

CLIENT_SRC = main_client.cpp \
             sources/network/client.cpp \
             sources/network/net_utils.cpp \
             sources/models/Model.cpp \
             sources/models/Architecture.cpp \
             sources/models/Layer.cpp \
             sources/serializer/serializer.cpp \
             sources/network/connection.cpp \
             sources/protocol/*.cpp

SERVER_BIN = server_app
CLIENT_BIN = client_app

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN):
	$(CXX) $(CXXFLAGS) $(SERVER_SRC) -o $(SERVER_BIN)

$(CLIENT_BIN):
	$(CXX) $(CXXFLAGS) $(CLIENT_SRC) -o $(CLIENT_BIN)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)

run_server: $(SERVER_BIN)
	./$(SERVER_BIN)

run_client: $(CLIENT_BIN)
	./$(CLIENT_BIN)
