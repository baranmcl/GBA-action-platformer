CXX := g++
CXXFLAGS := -std=c++17 -O0 -g -Wall -Wextra -I../include -I.
TEST_SRC  := $(wildcard test_*.cpp)
LOGIC_SRC := $(wildcard ../src/logic/*.cpp)
BIN := run_tests
all: purity $(BIN)
	./$(BIN)
purity:
	cd .. && python tools/check_logic_purity.py
$(BIN): $(TEST_SRC) $(LOGIC_SRC)
	$(CXX) $(CXXFLAGS) $(TEST_SRC) $(LOGIC_SRC) -o $(BIN)
clean:
	rm -f $(BIN)
.PHONY: all clean purity
