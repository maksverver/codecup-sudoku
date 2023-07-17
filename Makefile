#CXXFLAGS=-std=c++20 -O2 -Wall -Wextra
CXXFLAGS=-std=c++20 -Og -g -D_GLIBCXX_DEBUG -Wall -Wextra

BINS=output/player output/solver

COMMON_OBJS=build/check.o build/random.o build/state.o
PLAYER_OBJS=$(COMMON_OBJS) build/player.o
SOLVER_OBJS=$(COMMON_OBJS) build/solver.o

COMBINED_SRCS=src/check.h src/check.cc src/random.h src/random.cc src/state.h src/state.cc src/player.cc

all: $(BINS)

build/check.o:  src/check.cc src/check.h;        $(CXX) $(CXXFLAGS) -c $< -o $@
build/common.o: src/common.cc src/common.h;      $(CXX) $(CXXFLAGS) -c $< -o $@
build/random.o: src/random.cc src/random.h;      $(CXX) $(CXXFLAGS) -c $< -o $@
build/state.o:  src/state.cc src/state.h;        $(CXX) $(CXXFLAGS) -c $< -o $@

build/player.o: src/player.cc;                   $(CXX) $(CXXFLAGS) -c $< -o $@
build/solver.o: src/solver.cc;                   $(CXX) $(CXXFLAGS) -c $< -o $@

output/player: $(PLAYER_OBJS)
	$(CXX) $(CXXFLAGS) $(PLAYER_OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

output/solver: $(SOLVER_OBJS)
	$(CXX) $(CXXFLAGS) $(SOLVER_OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

output/combined-player.cc: $(COMBINED_SRCS)
	./combine-sources.sh $(COMBINED_SRCS) > $@

output/combined-player: output/combined-player.cc
	$(CXX) $(CXXFLAGS) -o $@ $<  $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(BINS) build/*.o output/combined-player.cc output/combined-player
