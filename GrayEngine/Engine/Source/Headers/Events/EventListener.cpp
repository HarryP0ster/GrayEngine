#include <pch.h>
#include "EventListener.h"

EventListener* EventListener::_instance = nullptr;

EventListener* EventListener::GetListener() //Singleton
{
	if (_instance == nullptr)
		_instance = new EventListener();
	return EventListener::_instance;
}

void EventListener::notify(const EventBase& event)
{
	switch (event.type)
	{
		case EventType::Custom:
			for (const auto& callback : observers_custom[event.name])
				callback(event.para);
			break;
		default:
			for (const auto& callback : observers_engine[event.type])
				callback(event.para);
			break;
	}
}

void EventListener::pushEvent(const char* name, const std::vector<double> para)
{
	EventBase event;
	event.type = EventType::Custom;
	event.name = name;
	event.para = para;

	EventQueue.push(event);
}

void EventListener::pushEvent(const EventType& type, const std::vector<double> para)
{
	if (type == EventType::Custom)
		throw std::runtime_error("Use const char* to define custom event!");
	EventBase event;
	event.type = type;
	event.para = para;

	EventQueue.push(event);
}

void EventListener::blockEvents(bool engineEventsEnabled, bool customEventsEnabled)
{
	bAllowEvents = engineEventsEnabled;
	bAllowCustomEvents = customEventsEnabled;
}

bool EventListener::pollEngineEvents()
{
	if (!bAllowEvents && !bAllowCustomEvents) return false;

	while (EventQueue.size() > 0)
	{
		if ((bAllowEvents && EventQueue.front().type != EventType::Custom) || (bAllowCustomEvents && EventQueue.front().type == EventType::Custom))
			notify(EventQueue.front());
		EventQueue.pop();
	}

	return true;
}