#include "NavtexMessageParser.h"
#include <string.h>

void NavtexMessageParser::reset() {
  state_ = State::Search;
  syncLength_ = headerLength_ = endLength_ = 0;
  memset(sync_, 0, sizeof(sync_));
  memset(header_, 0, sizeof(header_));
  memset(endMatch_, 0, sizeof(endMatch_));
  memset(&working_, 0, sizeof(working_));
  memset(messages_, 0, sizeof(messages_));
  count_ = writeIndex_ = 0;
  collecting_ = false;
  completedCount_ = 0;
}

bool NavtexMessageParser::get(uint8_t index, Message& out) const {
  if (index >= count_) return false;
  const uint8_t oldest = count_ == kMaxMessages ? writeIndex_ : 0;
  const uint8_t slot = static_cast<uint8_t>((oldest + index) % kMaxMessages);
  out = messages_[slot];
  return true;
}

void NavtexMessageParser::appendBody(char ch) {
  const size_t used = strlen(working_.body);
  if (used + 1 >= sizeof(working_.body)) return;
  working_.body[used] = ch;
  working_.body[used + 1] = '\0';
}

void NavtexMessageParser::finishMessage() {
  messages_[writeIndex_] = working_;
  writeIndex_ = static_cast<uint8_t>((writeIndex_ + 1) % kMaxMessages);
  if (count_ < kMaxMessages) ++count_;
  ++completedCount_;
  memset(&working_, 0, sizeof(working_));
  state_ = State::Search;
  syncLength_ = headerLength_ = endLength_ = 0;
  collecting_ = false;
}

void NavtexMessageParser::push(char input) {
  // NAVTEX text is conventionally upper-case. Ignore NUL/control noise while
  // retaining CR/LF in the message body for readable line breaks.
  char ch = input;
  if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');

  if (state_ == State::Search) {
    if (ch == "ZCZC"[syncLength_]) {
      sync_[syncLength_++] = ch;
      if (syncLength_ == 4) {
        state_ = State::Header;
        syncLength_ = headerLength_ = 0;
        memset(&working_, 0, sizeof(working_));
      }
    } else {
      syncLength_ = (ch == 'Z') ? 1 : 0;
      if (syncLength_) sync_[0] = 'Z';
    }
    return;
  }

  if (state_ == State::Header) {
    if (ch == '\r' || ch == '\n' || ch < 32 || ch > 126) return;
    if (headerLength_ < 4) header_[headerLength_++] = ch;
    if (headerLength_ == 4) {
      working_.station = header_[0];
      working_.subject = header_[1];
      working_.serial[0] = header_[2];
      working_.serial[1] = header_[3];
      working_.serial[2] = '\0';
      working_.receivedAtMs = millis();
      state_ = State::Body;
      collecting_ = true;
      endLength_ = 0;
    }
    return;
  }

  // In body state, detect NNNN without losing characters on partial matches.
  if (ch == "NNNN"[endLength_]) {
    endMatch_[endLength_++] = ch;
    if (endLength_ == 4) {
      finishMessage();
    }
    return;
  }

  if (endLength_) {
    for (uint8_t i = 0; i < endLength_; ++i) appendBody(endMatch_[i]);
    endLength_ = 0;
    if (ch == 'N') {
      endMatch_[endLength_++] = ch;
      return;
    }
  }

  if (ch == '\r' || ch == '\n' || (ch >= 32 && ch <= 126)) appendBody(ch);
}
