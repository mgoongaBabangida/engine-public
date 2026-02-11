#pragma once

//---------------------------------------------------------
struct GameLogic
{
  enum class GameState
  {
    SPANISH_TO_MOVE,
    PIRATE_TO_MOVE
  };

  enum class Side
  {
    BOTH,
    SPANISH,
    PIRATE
  };

  static const uint32_t                     m_ship_quantity = 4;
  static const uint32_t                     m_ship_quantity_pirate = 6;

  bool IsSpanishToMove() const { return m_game_state == GameState::SPANISH_TO_MOVE; }
  bool IsPirateToMove() const { return m_game_state == GameState::PIRATE_TO_MOVE; }

  bool IsPirateSide() const { return m_side == Side::PIRATE; }
  bool IsSpanishSide() const { return m_side == Side::SPANISH; }
  bool IsBothSide() const { return m_side == Side::BOTH; }

  GameState                m_game_state = GameState::SPANISH_TO_MOVE;
  Side                     m_side = Side::BOTH;
};