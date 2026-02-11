#pragma once

#include <base/interfaces.h>

class eMainContextBase;

//------------------------------------------
class SandBoxGameScript : public IScript
{
public:
  explicit SandBoxGameScript(eMainContextBase*);
  virtual void																			Update(float _tick);
protected:
  eMainContextBase* m_game = nullptr;
};