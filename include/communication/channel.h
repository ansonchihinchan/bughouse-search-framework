#pragma once

#include "communication/message.h"
#include "game/types.h"
#include <array>
#include <mutex>

// Holds the latest Message per side
class Channel {
public:
  void send(PlayerId player, Message message) {
    std::lock_guard<std::mutex> lock(mutex_);
    messages_[to_int(player)] = std::move(message);
  }

  Message latest(PlayerId player) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return messages_[to_int(player)];
  }

private:
  mutable std::mutex mutex_;
  std::array<Message, PLAYER_NO> messages_{};
};