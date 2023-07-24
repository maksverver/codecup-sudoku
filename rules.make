# Common build rules.
#
# Don't invoke this file directly. It is meant to be included in other files.

BINARIES=$(BIN)player $(BIN)solver

COMMON_HDRS=$(SRC)analysis.h $(SRC)check.h $(SRC)random.h $(SRC)state.h
COMMON_SRCS=$(SRC)analysis.cc $(SRC)check.cc $(SRC)random.cc $(SRC)state.cc
COMMON_OBJS=$(OBJ)analysis.o $(OBJ)check.o $(OBJ)random.o $(OBJ)state.o
PLAYER_OBJS=$(COMMON_OBJS) $(OBJ)player.o
SOLVER_OBJS=$(COMMON_OBJS) $(OBJ)solver.o

# Note that headers must be included in dependency order.
COMBINED_SRCS=$(SRC)check.h $(SRC)check.cc $(SRC)random.h $(SRC)random.cc $(SRC)state.h $(SRC)state.cc $(SRC)memo.h $(SRC)analysis.h $(SRC)analysis.cc $(SRC)player.cc

all: $(BINARIES)

$(OBJ)analysis.o: $(SRC)analysis.cc $(SRC)analysis.h $(SRC)memo.h $(SRC)random.h $(SRC)state.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
$(OBJ)check.o:  $(SRC)check.cc    $(SRC)check.h;  $(CXX) $(CXXFLAGS) -c $< -o $@
$(OBJ)random.o: $(SRC)random.cc   $(SRC)random.h; $(CXX) $(CXXFLAGS) -c $< -o $@
$(OBJ)state.o:  $(SRC)state.cc    $(SRC)state.h;  $(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ)player.o: $(SRC)player.cc $(COMMON_HDRS);    $(CXX) $(CXXFLAGS) -c $< -o $@
$(OBJ)solver.o: $(SRC)solver.cc $(COMMON_HDRS);    $(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN)player: $(PLAYER_OBJS)
	$(CXX) $(CXXFLAGS) $(PLAYER_OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(BIN)solver: $(SOLVER_OBJS)
	$(CXX) $(CXXFLAGS) $(SOLVER_OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(OUT)combined-player.cc: $(COMBINED_SRCS)
	./combine-sources.sh $(COMBINED_SRCS) > $@

$(BIN)combined-player: $(OUT)combined-player.cc
	$(CXX) $(CXXFLAGS) -o $@ $<  $(LDFLAGS) $(LDLIBS)

player: $(BIN)player
solver: $(BIN)solver
combined: $(BIN)combined-player

clean:
	rm -f $(BINARIES) $(OBJ)*.o $(OUT)combined-player.cc $(BIN)combined-player

.PHONY: all clean player solver combined
