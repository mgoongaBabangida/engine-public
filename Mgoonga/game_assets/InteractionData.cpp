#include "stdafx.h"
#include "InteractionData.h"

InteractionScript::InteractionScript(eMainContextBase* _game)
  : m_game(_game)
{
}

//--------------------------------------------------------------------------------
InteractionScript::InteractionScript(eMainContextBase* _game, const InteractionData& _data, dbb::OBB _volume)
  : m_game(_game), m_interaction_data(_data), m_volume(_volume)
{
}

InteractionScript::~InteractionScript()
{
}
