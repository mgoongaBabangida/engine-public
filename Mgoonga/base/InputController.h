#pragma once

#include <deque>
#include <vector>

#include "interfaces.h"

//-------------------------------------------------------
class DLL_BASE eInputController
{
public:
	eInputController();
  virtual void  Update();
	virtual void	OnMouseMove(uint32_t x, uint32_t y, KeyModifiers);
	virtual bool	OnKeyJustPressed(uint32_t asci, KeyModifiers _modifier = KeyModifiers::NONE);
	virtual bool	OnKeyPress();
	virtual bool	OnKeyRelease(uint32_t asci);
	virtual void	OnMousePress(uint32_t x, uint32_t y, bool left , KeyModifiers = KeyModifiers::NONE);
  virtual void  OnMouseDoublePress(uint32_t x, uint32_t y, bool left, KeyModifiers _modifier = KeyModifiers::NONE);
	virtual void	OnMouseRelease(KeyModifiers = KeyModifiers::NONE);
	virtual void	OnMouseWheel(int32_t x, int32_t y, KeyModifiers = KeyModifiers::NONE);
  virtual void  OnDropEvent(uint32_t x, uint32_t y, const std::string& _file_path);

	bool			IsAnyKeyPressed() const;

	void			AddObserver(IInputObserver*, ePriority);
	void			DeleteObserver(IInputObserver*);

private:
	//---------------------------------------------------------------------------
	template <typename Func>
	void NotifyObservers(const std::string& eventType, Func&& func)
	{
    bool taken = false;

    for (auto& observer : observersPriority)
    {
      if (observer)
      {
        bool result = func(observer);
        LogEvent(eventType, typeid(*observer).name(),"STRONG", result);
        if (result)
        {
          taken = true;
          break;
        }
      }
    }

    if (!taken)
    {
      for (auto& observer : observers)
      {
        if (observer)
        {
          bool result = func(observer);
          LogEvent(eventType, typeid(*observer).name(),"WEAK", result);
          if (result) break;
        }
      }
    }

    for (auto& observer : observersAlways)
    {
      if (observer)
      {
        bool result = func(observer);
        LogEvent(eventType, typeid(*observer).name(), "ALWAYS", result);
      }
    }
	}

	void LogEvent(const std::string& eventType, const std::string& observerType, const std::string& priority, bool returnValue);

	std::vector<IInputObserver*>	observersPriority;
	std::deque<IInputObserver*>		observers;
	std::vector<IInputObserver*>	observersAlways;

	std::vector<bool>				keysPressed;
};
