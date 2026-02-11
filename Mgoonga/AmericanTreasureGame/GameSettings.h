#pragma once

#include <tcp_lib/TCPConnection.h>
#include <math/Timer.h>

class SimpleGeometryMesh;
class LineMesh;
class ITcpAgent;

//---------------------------------------------------------
struct VisualSettings // settings connected to back end rendering
{
  SimpleGeometryMesh*                m_choice_circle = nullptr;
  float                              m_hdr_strangth = 8.0f;
  LineMesh*                          m_path_mesh = nullptr;

  std::vector<float>                 m_texture_scales;
  std::vector<std::shared_ptr<Text>> texts; //debug
  float                              m_height_scale = 2.0f;
  float                              m_max_height = 1.0f;
  float                              m_water_level = 2.0f;
  float                              m_texturing_water_level = 0.5f;
  float                              m_ship_height_level = 0;
};

//---------------------------------------------------------
struct NetworkSettings
{
  enum class Mode
  {
    SERVER,
    CLIENT,
    LOCAL
  };

  enum class MessageType : uint32_t
  {
    MOVE,
  };

  Mode                                m_mode = Mode::LOCAL;
  std::unique_ptr <ITcpAgent>				  m_tcpAgent;
  std::unique_ptr<math::Timer>			  m_tcpTimer;
};

//---------------------------------------------------------
struct SoundSettings
{
  RemSnd* m_seagull_sound;
};