//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2017
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "td/db/binlog/detail/BinlogEventsProcessor.h"

#include "td/utils/logging.h"

#include <algorithm>

namespace td {
namespace detail {
void BinlogEventsProcessor::do_event(BinlogEvent &&event) {
  offset_ = event.offset_;
  auto fixed_id = event.id_ * 2;
  if ((event.flags_ & BinlogEvent::Flags::Rewrite) && !ids_.empty() && ids_.back() >= fixed_id) {
    auto it = std::lower_bound(ids_.begin(), ids_.end(), fixed_id);
    if (it == ids_.end() || *it != fixed_id) {
      LOG(FATAL) << "Ignore rewrite logevent";
      return;
    }
    auto pos = it - ids_.begin();
    total_raw_events_size_ -= static_cast<int64>(events_[pos].raw_event_.size());
    if (event.type_ == BinlogEvent::ServiceTypes::Empty) {
      *it += 1;
      empty_events_++;
      events_[pos].clear();
    } else {
      event.flags_ &= ~BinlogEvent::Flags::Rewrite;
      total_raw_events_size_ += static_cast<int64>(event.raw_event_.size());
      events_[pos] = std::move(event);
    }
  } else if (event.type_ == BinlogEvent::ServiceTypes::Empty) {
    // just skip this event
  } else {
    CHECK(ids_.empty() || ids_.back() < fixed_id);
    last_id_ = event.id_;
    total_raw_events_size_ += static_cast<int64>(event.raw_event_.size());
    total_events_++;
    ids_.push_back(fixed_id);
    events_.emplace_back(std::move(event));
  }

  if (total_events_ > 10 && empty_events_ * 4 > total_events_ * 3) {
    compactify();
  }
}

void BinlogEventsProcessor::compactify() {
  CHECK(ids_.size() == events_.size());
  auto ids_from = ids_.begin();
  auto ids_to = ids_from;
  auto events_from = events_.begin();
  auto events_to = events_from;
  for (; ids_from != ids_.end(); ids_from++, events_from++) {
    if ((*ids_from & 1) == 0) {
      *ids_to++ = *ids_from;
      *events_to++ = std::move(*events_from);
    }
  }
  ids_.erase(ids_to, ids_.end());
  events_.erase(events_to, events_.end());
  total_events_ = ids_.size();
  empty_events_ = 0;
  CHECK(ids_.size() == events_.size());
}
}  // namespace detail
}  // namespace td
