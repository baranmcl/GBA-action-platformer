CXX := g++
CXXFLAGS := -std=c++17 -O0 -g -Wall -Wextra -I../include -I.
TEST_SRC  := $(wildcard test_*.cpp)
LOGIC_SRC := $(wildcard ../src/logic/*.cpp)
BIN := run_tests
LEVEL_TXT := $(wildcard ../tools/levels/*.txt)
levels:
	@for f in $(LEVEL_TXT); do \
	  python ../tools/build_level.py $$f ../include/game/levels/$$(basename $${f%.txt}).h; \
	done
all: purity levels $(BIN)
	./$(BIN)
purity:
	cd .. && python tools/check_logic_purity.py
$(BIN): $(TEST_SRC) $(LOGIC_SRC)
	$(CXX) $(CXXFLAGS) $(TEST_SRC) $(LOGIC_SRC) -o $(BIN)
clean:
	rm -f $(BIN)
.PHONY: all clean purity levels
