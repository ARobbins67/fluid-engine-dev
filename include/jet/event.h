// Copyright (c) 2016 Doyub Kim

#ifndef INCLUDE_JET_EVENT_H_
#define INCLUDE_JET_EVENT_H_

#include <functional>
#include <map>

namespace jet {

typedef size_t EventToken;

static const EventToken kEmptyEventToken = 0;

template <typename ...EventArgTypes>
class Event {
 public:
    typedef std::function<void(EventArgTypes...)> CallbackType;

    void operator()(EventArgTypes... args);

    EventToken operator+=(const CallbackType& callback);

    void operator-=(EventToken token);

 private:
    EventToken _lastToken = 0;
    std::map<EventToken, CallbackType> _callbacks;
};

}  // namespace jet

#include "detail/event-inl.h"

#endif  // INCLUDE_JET_EVENT_H_