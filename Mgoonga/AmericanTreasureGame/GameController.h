#pragma once

#include <game_assets/GameScript.h>

#include "GameLogic.h"
#include "Hex.h"
#include "ShipScript.h"
#include "BaseScript.h"
#include "GameSettings.h"
#include "GameGUI.h"

class IWindowImGui;

//-----------------------------------------------------------------------------------------------
class GameController : public GameScript
{
public:
  GameController(eMainContextBase* _game,
                 eModelManager* _modelManager,
                 eTextureManager* _texManager,
                 eSoundManager* _soundManager,
                 eOpenGlRenderPipeline& _pipeline,
                 Camera& _camera,
                 IWindowImGui* = nullptr);

  virtual ~GameController();

  virtual void Initialize() override;
  virtual void Update(float _tick) override;

  virtual bool OnKeyPress(uint32_t _asci, KeyModifiers _modifier);
  virtual bool OnMouseMove(int32_t _x, int32_t _y, KeyModifiers _modifier) override;
  virtual bool OnMousePress(int32_t x, int32_t y, bool left, KeyModifiers _modifier) override;
  virtual bool OnMouseRelease(KeyModifiers _modifier) override;

protected:
  void OnObjectPickedWithLeft(std::shared_ptr<eObject> _picked);
  void OnObjectPickedWithRight(std::shared_ptr<eObject> _picked);
  void OnFrameMoved(std::shared_ptr<GUI> _frame);
  void OnShipCameToBase(std::shared_ptr<eObject>, const std::string& _base_name);

  void OnShoot(const eObject* _shooter);
  void OnGetHit(const eObject* _target);

  void _OnConnectionEstablished(const dbb::TCPConnection& _connection);
  void _OnTCPMessageRecieved(const std::vector<uint32_t>);
  void _OnShipDeleted(shObject);

  void _InitializeDiceLogicAndVisual();
  void _InitializeShipIcons();
  void _InitializeHexes();
  void _InitializeTerrain();
  void _InitializeShips();
  void _InitializeBases();
  void _InitializeGoldenFrame();
  void _InitializeSounds();
  void _InitializeDebug();

  void _InitializeGLData(); // GL-thread

  void _InstallTcpServer();
  void _InstallTcpClient();

  bool            _CurrentShipHasMoved() const;
  const Texture*  _GetDiceTexture() const;

  void            _UpdateLight(float _tick);
  void            _UpdatePathVisual();
  void            _UpdateWarrning(const std::string& _message);
  void            _SetDestinationFromCurrentPath();
  void            _UpdateCurrentPath(int32_t _x, int32_t _y);
  void            _UpdateTextPath();
  void            _SendMoveMsg();
  void            _UpdateDebug();

  std::vector<uint32_t>     _GetCurPathIndices();
  Hex*                      _GetCurHex();
  void                      _DebugHexes();
  eShipScript*              _GetShipScript(const eObject*) const;
  size_t                    _GetPirateShipIndex(std::shared_ptr<eObject>);
  size_t                    _GetSpanishShipIndex(std::shared_ptr<eObject>);
  std::shared_ptr<eObject>  _GetPirateShipByIndex(size_t);
  std::shared_ptr<eObject>  _GetSpanishShipByIndex(size_t);
  eObject*                  _GetObject(eShipScript*) const;

protected:
  GameLogic                                 m_game_logic;

  shObject                                  m_terrain;
  std::vector<eShipScript*>                 m_ships;
  std::vector<eShipScript*>                 m_ships_pirate;
  eObject*                                  m_target = nullptr;

  glm::vec2                                 m_cursor_pos;
  bool                                      m_right_button_pressed = false;

  std::vector<Hex>                          m_hexes;
  std::deque<Hex*>                          m_current_path;

  std::array<bool, GameLogic::m_ship_quantity>         m_has_moved;
  std::array<bool, GameLogic::m_ship_quantity_pirate>  m_has_moved_pirate;

  uint32_t                                  m_focused_index = -1;
  uint32_t                                  m_has_gold_index = -1;

  uint32_t                                  m_current_dice = 1;
  bool                                      m_dice_rolled = false;

  GameGUI                                   m_game_gui;
  VisualSettings                            m_visual_settings;
  NetworkSettings                           m_network_settings;
  SoundSettings                             m_sound_settings;

  //debug
  IWindowImGui*                             m_debug_window = nullptr;
};