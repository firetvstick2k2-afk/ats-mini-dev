#pragma once

#include <Arduino.h>

// NAVTEX message framing after the SITOR-B / CCIR-476 character decoder.
// This class deliberately does not perform RF/audio demodulation or FEC;
// those stages feed recovered characters through push().
class NavtexMessageParser {
public:
  static constexpr uint8_t kMaxMessages = 8;
  static constexpr size_t kBodyBytes = 768;

  struct Message {
    char station = '?';
    char subject = '?';
    char serial[3] = {};
    char body[kBodyBytes] = {};
    uint32_t receivedAtMs = 0;
  };

  void reset();
  void push(char ch);
  uint8_t count() const { return count_; }
  bool get(uint8_t index, Message& out) const;
  bool inMessage() const { return collecting_; }
  uint32_t completedCount() const { return completedCount_; }

private:
  enum class State : uint8_t { Search, Header, Body };
  void appendBody(char ch);
  void finishMessage();

  State state_ = State::Search;
  char sync_[4] = {};
  uint8_t syncLength_ = 0;
  char header_[5] = {};
  uint8_t headerLength_ = 0;
  char endMatch_[4] = {};
  uint8_t endLength_ = 0;
  Message working_{};
  Message messages_[kMaxMessages]{};
  uint8_t count_ = 0;
  uint8_t writeIndex_ = 0;
  bool collecting_ = false;
  uint32_t completedCount_ = 0;
};
