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
#include "state.h"

#include <cstdint>
#include <limits>
#include <iostream>
#include <string_view>

// Line-buffered log entry.
//
// Always starts with a tag followed by a space, and ends with a newline.
class LogStream {
public:
  LogStream(std::string_view tag, std::ostream &os = std::clog) : os(os) {
    if (!tag.empty()) os << tag << ' ';
  }

  ~LogStream() { os << std::endl; }

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

// Log the state at the beginning of a turn, and how much time the player thinks
// it has used so far.
inline void LogTurn(int turn, const State &state, int64_t time_used_ms) {
  LogStream("TURN") << turn << ' ' << state.DebugString() << ' ' << time_used_ms;
}

// Log the number of solutions that remain.
inline void LogSolutions(int64_t count, bool complete) {
  LogStream("SOLUTIONS") << count << (complete ? "+" : "");
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
// ms_total >= ms_enumerate + ms_analyze
inline void LogTime(int64_t ms_total, int64_t ms_enumerate, int64_t ms_analyze) {
  LogStream("TIME") << ms_total
      << " ENUMERATE " << ms_enumerate
      << " ANALYZE " << ms_analyze;
}

#endif  // ndef LOGGING_H_INCLUDED
