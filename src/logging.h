// Functions and definitions to log player information to standard error.
//
// There are several reasons for splitting this off from player.cc:
//
//  1. To separate the logging logic from the syntax of the log files.
//
//  2. To ensure that log files have a uniform, machine parseable structure
//     which facilitates log file analysis after a competition has been played.
//
//     For example, `grep ^TURN playerlog.txt` lists the state at the beginning
//     of each turn, and `grep ^IO playerlog.txt` the moves sent and received.
//
//  3. Putting all logging logic into a single header file allows this file
//     to serve as documentation of the kind of statements that may appear in
//     log files.

#ifndef LOGGING_H_INCLUDED
#define LOGGING_H_INCLUDED

#include "analysis.h"
#include "random.h"
#include "state.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <iostream>
#include <string_view>

// Granularity of time used in log files.
using log_duration_t = std::chrono::milliseconds;

// Line-buffered log entry.
//
// Always starts with a tag followed by a space, and ends with a newline.
class LogStream {
public:
  LogStream(std::string_view tag, std::ostream &os = std::clog) : os(os) {
    if (!tag.empty()) os << tag << ' ';
  }

  ~LogStream() { os << std::endl; }

  LogStream &operator<<(const log_duration_t &value) {
    // I could add an `ms` suffix, but logs are shorter and easier to parse without it.
    os << value.count();
    return *this;
  }

  template<class T>
  LogStream &operator<<(const T &value) {
    os << value;
    return *this;
  }

private:
  std::ostream &os;
};

// Log an arbitrary informational message.
inline LogStream LogInfo()    { return LogStream("INFO"); }

// Log an arbitrary warning.
inline LogStream LogWarning() { return LogStream("WARNING"); }

// Log an arbitrary error message. This is typically followed by the player
// exiting with a nonzero status code.
inline LogStream LogError()   { return LogStream("ERROR"); }

// Log the player ID, usually once at the start of the program.
inline void LogId(std::string_view player_name) {
  LogStream("ID") << player_name
      << " (" << std::numeric_limits<size_t>::digits << " bit)"

#ifdef __VERSION__
      << " (compiler v" __VERSION__ << ")"
#endif

#ifdef GIT_COMMIT
      << " (commit " GIT_COMMIT
  #if GIT_DIRTY
      << "; uncommitted changes"
  #endif
      << ")"
#endif
  ;
}

inline void LogSeed(const rng_seed_t &seed) {
  LogStream("SEED") << FormatSeed(seed);
}

// Log the state at the beginning of a turn, and how much time the player thinks
// it has used so far.
inline void LogTurn(int turn, const State &state, log_duration_t time_used) {
  LogStream("TURN") << turn << ' ' << state.DebugString() << ' ' << time_used;
}

// Log the number of solutions that remain.
inline void LogSolutions(int64_t count, bool complete) {
  LogStream("SOLUTIONS") << count << (complete ? "" : "+");
}

// Log the move string that the player is about to send.
inline void LogSending(std::string_view s) {
  LogStream("IO") << "SEND [" << s << "]";
}

// Log the move string that the player has just received.
inline void LogReceived(std::string_view s) {
  LogStream("IO") << "RCVD [" << s << "]";
}

// Log the outcome of the game according to analysis.
inline void LogOutcome(Outcome o) {
  LogStream("OUTCOME") << o;
}

// Log the time taken this turn.
// total >= enumerate + analyze
inline void LogTime(log_duration_t total, log_duration_t enumerate, log_duration_t analyze) {
  LogStream("TIME") << total << " ENUMERATE " << enumerate << " ANALYZE " << analyze;
}

// Log the time spend paused.
// This is an upper bound on the time spent by the opponent.
inline void LogPause(log_duration_t interval, log_duration_t total) {
  LogStream("PAUSE") << interval << " " << total;
}

#endif  // ndef LOGGING_H_INCLUDED
