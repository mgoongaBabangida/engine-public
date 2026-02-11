#include "stdafx.h"
#include "InputController.h"
#include <chrono>
#include <iostream>

//------------------------------------------------------------------
eInputController::eInputController()
{
	for(int i = 0; i < ASCII_LAST_ENUM; ++i)
	{
		keysPressed.push_back(false);
	}
}

//------------------------------------------------------------------
void eInputController::Update()
{
	NotifyObservers("Update", [&](auto observer) { return observer->OnUpdate(); });
}

//------------------------------------------------------------------
void eInputController::OnMouseMove(uint32_t x, uint32_t y, KeyModifiers _modifiers)
{
	NotifyObservers("OnMouseMove", [&](auto observer) { return observer->OnMouseMove(x, y, _modifiers); });
}

//------------------------------------------------------------------
bool eInputController::OnKeyJustPressed(uint32_t _asci, KeyModifiers _modifier)
{
	if (_asci >= keysPressed.size()) return true;

	keysPressed[_asci] = true;

	NotifyObservers("OnKeyJustPressed", [&](auto observer) { return observer->OnKeyJustPressed(_asci, _modifier); });

	return true;
}

//------------------------------------------------------------------
bool eInputController::OnKeyPress()
{
	NotifyObservers("OnKeyPress", [&](auto observer) { return observer->OnKeyPress(keysPressed, KeyModifiers::NONE); });
	return true;
}

//----------------------------------------------------------------------
bool eInputController::OnKeyRelease(uint32_t _asci)
{
	if (_asci >= keysPressed.size()) return true;

	if (keysPressed[_asci])
		keysPressed[_asci] = false;

	NotifyObservers("OnKeyRelease", [&](auto observer) { return observer->OnKeyRelease((ASCII)_asci, keysPressed, KeyModifiers::NONE); });

	return true;
}

//----------------------------------------------------------------------
void eInputController::OnMousePress(uint32_t x, uint32_t y, bool left, KeyModifiers _modifier)
{
	NotifyObservers("OnMousePress", [&](auto observer) { return observer->OnMousePress(x, y, left, _modifier); });
}

//----------------------------------------------------------------------
void eInputController::OnMouseDoublePress(uint32_t x, uint32_t y, bool left, KeyModifiers _modifier)
{
	NotifyObservers("OnMouseDoublePress", [&](auto observer) { return observer->OnMouseDoublePress(x, y, left, _modifier); });
}

//----------------------------------------------------------------------
void eInputController::OnMouseRelease(KeyModifiers _modifier)
{
	NotifyObservers("OnMouseRelease", [&](auto observer) { return observer->OnMouseRelease(_modifier); });
}

//----------------------------------------------------------------------
void eInputController::OnMouseWheel(int32_t _x, int32_t _y, KeyModifiers _modifier)
{
	NotifyObservers("OnMouseWheel", [&](auto observer) { return observer->OnMouseWheel(_x, _y, _modifier); });
}

//------------------------------------------------------------------
void eInputController::OnDropEvent(uint32_t x, uint32_t y, const std::string& _file_path)
{
	NotifyObservers("OnDropEvent", [&](auto observer) { return observer->OnDropEvent(x, y, _file_path); });
}

//----------------------------------------------------------------------
bool eInputController::IsAnyKeyPressed() const
{
	for(bool isPressed : keysPressed)
		if(isPressed) return true;

	return false;
}

//---------------------------------------------------------------------------
void eInputController::AddObserver(IInputObserver* _obs, ePriority _priority)
{
	//@todo check if it is already there
	switch(_priority)
	{
		case MONOPOLY:	observersPriority.push_back(_obs);	break;
		case STRONG:		observers.push_front(_obs);					break;
		case WEAK:			observers.push_back(_obs);					break;
		case ALWAYS:		observersAlways.push_back(_obs);		break;
	}
}

//---------------------------------------------------------------------------
void eInputController::DeleteObserver(IInputObserver* _obs)
{
	if(auto it = std::remove(observersPriority.begin(), observersPriority.end(), (_obs)); it!= observersPriority.end())
		observersPriority.erase(it);
	else if(auto it = std::remove(observers.begin(), observers.end(), (_obs)); it != observers.end())
		observers.erase(it);
	else if (auto it = std::remove(observersAlways.begin(), observersAlways.end(), (_obs)); it != observersAlways.end())
		observersAlways.erase(it);
}

//-----------------------------------------------------------------------------
void eInputController::LogEvent(const std::string& eventType, const std::string& observerType, const std::string& priority, bool returnValue)
{
	if (false /*eventType == "OnKeyJustPressed"*/)
	{
		// Get system time
		auto now = std::chrono::system_clock::now();
		std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

		// Log details
		std::cout << "[" << std::ctime(&nowTime) << "] "   // Time
			<< "Event: " << eventType << ", "
			<< "Observer Type: " << observerType << ", "
			<< "Priority Type: " << priority << ", "
			<< "Return Value: " << returnValue << "\n";
	}
}
