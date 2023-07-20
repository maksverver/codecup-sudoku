#CXXFLAGS=-std=c++20 -O3 -Wall -Wextra -march=native
CXXFLAGS=-std=c++20 -Og -g -D_GLIBCXX_DEBUG -Wall -Wextra

BINS=output/player output/solver

COMMON_HDRS=src/analysis.h src/check.h src/random.h src/state.h
COMMON_SRCS=src/analysis.cc src/check.cc src/random.cc src/state.cc
COMMON_OBJS=build/analysis.o build/check.o build/random.o build/state.o
PLAYER_OBJS=$(COMMON_OBJS) build/player.o
SOLVER_OBJS=$(COMMON_OBJS) build/solver.o

COMBINED_SRCS=$(COMMON_HDRS) $(COMMON_SRCS) src/player.cc

all: $(BINS)

build/analysis.o: src/analysis.cc src/analysis.h src/random.h src/state.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/check.o:  src/check.cc    src/check.h;  $(CXX) $(CXXFLAGS) -c $< -o $@
build/random.o: src/random.cc   src/random.h; $(CXX) $(CXXFLAGS) -c $< -o $@
build/state.o:  src/state.cc    src/state.h;  $(CXX) $(CXXFLAGS) -c $< -o $@

build/player.o: src/player.cc $(COMMON_HDRS);    $(CXX) $(CXXFLAGS) -c $< -o $@
build/solver.o: src/solver.cc $(COMMON_HDRS);    $(CXX) $(CXXFLAGS) -c $< -o $@

output/player: $(PLAYER_OBJS)
	$(CXX) $(CXXFLAGS) $(PLAYER_OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

output/solver: $(SOLVER_OBJS)
	$(CXX) $(CXXFLAGS) $(SOLVER_OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

output/combined-player.cc: $(COMBINED_SRCS)
	./combine-sources.sh $(COMBINED_SRCS) > $@

output/combined-player: output/combined-player.cc
	$(CXX) $(CXXFLAGS) -o $@ $<  $(LDFLAGS) $(LDLIBS)

player: output/player
solver: output/solver
combined: output/combined-player

clean:
	rm -f $(BINS) build/*.o output/combined-player.cc output/combined-player

.PHONY: all clean player solver combined
