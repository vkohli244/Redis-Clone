CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g

COMMON_SRCS := utils.cpp

SERVER_SRCS := server.cpp $(COMMON_SRCS)
CLIENT_SRCS := client.cpp $(COMMON_SRCS)

SERVER_BIN := server
CLIENT_BIN := client

.PHONY: all clean run-server run-client

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRCS) utils.h
	$(CXX) $(CXXFLAGS) $(SERVER_SRCS) -o $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_SRCS) utils.h
	$(CXX) $(CXXFLAGS) $(CLIENT_SRCS) -o $(CLIENT_BIN)

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
	rm -rf *.dSYM
