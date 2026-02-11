#include "stdafx.h"
#include "ParticleSystem.h"
#include "Random.h"
#include <base/base.h>

#include <algorithm>
#include <chrono>
#include <random>

//------------------------------------------------------------------------------------
ParticleSystem::ParticleSystem(float	 _pps,
															 float	 _speed,
															 float	 _gravityComplient,
															 float	 _lifeLength,
															 glm::vec3 _systemCenter,
															 const Texture*  _texture,
															 ISound*	 _sound,
															 size_t	 _num_rows_in_texture,
															 float	 _duration)
: IParticleSystem()
, m_duration(_duration)
, m_pps(static_cast<uint32_t>(_pps))
, m_speed(_speed)
, m_scale(0.05f, 0.05f, 0.05f)
, m_gravity_complient(_gravityComplient)
, m_life_length(_lifeLength / 1000)
, m_sound(_sound)
, m_num_rows_in_texture(_num_rows_in_texture)
, m_particles(MAX_PARTICLES, Particle{})
, m_texture(const_cast<Texture*>(_texture)) //@todo const_cast
{
	m_transform.setTranslation(_systemCenter);
  m_particles.resize(MAX_PARTICLES);
}

//--------------------------------------------------------------------------------------
ParticleSystem::~ParticleSystem()
{
	if (m_timer)
		m_timer->stop();
}

//-------------------------------------------------------------------------
void ParticleSystem::Start()
{
	if (m_timer)
		m_timer->stop();

	m_timer.reset(new math::Timer([this]()->bool { this->Update(); return true; }));

	m_clock.restart();

	if (m_sound)
		m_sound->Play();

	m_timer->start(30);
	srand(static_cast<unsigned int>(time(0)));
}

//-------------------------------------------------------------------------
void ParticleSystem::GenerateParticles(int64_t _tick)
{
	if(m_clock.timeElapsedMsc() < m_duration || m_loop)
	{
		int64_t msc = _tick + m_time_wo_new_particles;
		float new_particlesf = static_cast<float>(msc) / 1000.0f * static_cast<float>(m_pps);
		if (size_t new_particles = static_cast<size_t>(new_particlesf); new_particles)
		{
			for (size_t i = 0; i < new_particles; ++i)
				_emitParticles();
			m_time_wo_new_particles = 0;
		}
		else
			m_time_wo_new_particles = msc;

		//std::cout << "Generate " << m_time_wo_new_particles << std::endl;
	}
	else
	{
		if(m_sound && m_sound->isPlaying())
		{
			m_sound->Stop();
		}
	}
}

//------------------------------------------------------------------------------
void ParticleSystem::_emitParticles()
{
	glm::vec3 dir = _calculateParticles();
	glm::vec3 origin = m_transform.getTranslation();
	if (m_base_radius > 0.0f)
	{
		origin.x = origin.x + math::Random::RandomFloat(-m_base_radius, m_base_radius);
		origin.z = origin.z + math::Random::RandomFloat(-m_base_radius, m_base_radius);
	}

	if (m_cur_particles < MAX_PARTICLES)
	{
		m_particles[m_cur_particles].reset(origin, dir, m_gravity_complient, m_life_length * 1'000, 0, m_scale.x, static_cast<uint32_t>(m_num_rows_in_texture));
		++m_cur_particles;
	}
	else
	{
		auto prt = std::find_if(m_particles.begin(), m_particles.end(), [](Particle& pt) { return !pt.isAlive(); });
		if (prt != m_particles.end())
			prt->reset(origin, dir, m_gravity_complient, m_life_length * 1'000, 0, m_scale.x, static_cast<uint32_t>(m_num_rows_in_texture));
	}
}

//--------------------------------------------------------------------------------------
glm::vec3 ParticleSystem::_calculateParticles()
{
	if (m_type == CONE)
	{
		float theta = glm::mix(0.0f, m_cone_angle,	math::Random::RandomFloat(0.0f, 1.0f));
		float phi = glm::mix(0.0f, PI * 2.0f,				math::Random::RandomFloat(0.0f, 1.0f));

		glm::vec3 dir(0.0);
		dir.x = (sinf(theta) * cosf(phi)) * m_speed;
		dir.y = (cosf(theta))							* m_speed;
		dir.z = (sinf(theta) * sinf(phi)) * m_speed;
		dir = dir * math::Random::RandomFloat(1.0f - m_randomize_magnitude, 1.0f + m_randomize_magnitude);
		return glm::vec3(glm::vec4(dir,1.0f) * glm::toMat4(m_transform.getRotation()));
	}
	else if (m_type == SPHERE) //@todo also use speed
	{
		glm::vec3 dir(
			(math::Random::RandomFloat(0.0f, 1.0f) * 2.0f - 1.0f) * m_speed,
			(math::Random::RandomFloat(0.0f, 1.0f) * 2.0f - 1.0f) * m_speed,
			(math::Random::RandomFloat(0.0f, 1.0f) * 2.0f - 1.0f) * m_speed
		);
		return dir * math::Random::RandomFloat(1.0f - m_randomize_magnitude, 1.0f + m_randomize_magnitude);
	}
	assert("ParticleSystem::_calculateParticles");
	return {};
}

//-----------------------------------------------------------------------------------
std::vector<Particle>::iterator ParticleSystem::PrepareParticles(const glm::vec3& _cameraPosition) 
{
	// sort alive or not
	std::vector<Particle>::iterator n_end = m_particles.begin();
	
	//Find last alive and set distance
	while(n_end != m_particles.end() && n_end->isAlive())
	{
		n_end->setDistance(glm::length(glm::vec3(_cameraPosition - n_end->getPosition())));
		++n_end;
	}
	std::sort(m_particles.begin(), n_end, [](Particle& prt1, Particle& prt2) { return prt1.getDistance() > prt2.getDistance(); });
	return n_end;
}

//---------------------------------------------------------------------------------------
bool ParticleSystem::IsFinished()
{
	if (!m_loop)
		return m_clock.timeElapsedMsc() > m_duration + (m_life_length * 1000);
	else
		return false;
}

//------------------------------------------------------------------------------
bool ParticleSystem::IsStarted()
{
	return m_clock.isActive();
}

//-------------------------------------------------------------------------------------
void ParticleSystem::Reset()
{
	m_clock.reset();
}

//-------------------------------------------------------------------------------------
void ParticleSystem::SetSizeBezier(std::array<glm::vec3, 4> _bezier)
{
	m_particle_size_over_time.p0 = { (_bezier[0].x + 1.0f) / 2.0f, (_bezier[0].y + 1.0f) / 2.0f, 0.0f }; // ( +1) /2 from -1 1 to 0 1
	m_particle_size_over_time.p1 = { (_bezier[1].x + 1.0f) / 2.0f, (_bezier[1].y + 1.0f) / 2.0f, 0.0f };
	m_particle_size_over_time.p2 = { (_bezier[2].x + 1.0f) / 2.0f, (_bezier[2].y + 1.0f) / 2.0f, 0.0f };
	m_particle_size_over_time.p3 = { (_bezier[3].x + 1.0f) / 2.0f, (_bezier[3].y + 1.0f) / 2.0f, 0.0f };
	m_use_size_over_time = true;
	for (auto& p : m_particles)
		p.setScaleCurve(&m_particle_size_over_time);
}

//-------------------------------------------------------------------------------------
std::vector<glm::vec3> ParticleSystem::GetExtremsWorldSpace() const
{
	extremDots	dots;
	for (const Particle& prt : m_particles)
	{
		if (prt.isAlive())
		{
			glm::vec3 pos = prt.getPosition();
			if (pos.x > dots.MaxX)
				dots.MaxX = pos.x;
			if (pos.x < dots.MinX)
				dots.MinX = pos.x;
			if (pos.y > dots.MaxY)
				dots.MaxY = pos.y;
			if (pos.y < dots.MinY)
				dots.MinY = pos.y;
			if (pos.z > dots.MaxZ)
				dots.MaxZ = pos.z;
			if (pos.z < dots.MinZ)
				dots.MinZ = pos.z;
		}
	}

	std::vector<glm::vec3> ret;
	ret.push_back(glm::vec3(dots.MaxX, dots.MaxY, dots.MaxZ));
	ret.push_back(glm::vec3(dots.MaxX, dots.MaxY, dots.MinZ));
	ret.push_back(glm::vec3(dots.MinX, dots.MaxY, dots.MinZ));
	ret.push_back(glm::vec3(dots.MinX, dots.MaxY, dots.MaxZ));
	ret.push_back(glm::vec3(dots.MaxX, dots.MinY, dots.MaxZ));
	ret.push_back(glm::vec3(dots.MaxX, dots.MinY, dots.MinZ));
	ret.push_back(glm::vec3(dots.MinX, dots.MinY, dots.MinZ));
	ret.push_back(glm::vec3(dots.MinX, dots.MinY, dots.MaxZ));
	return ret;
}

//-------------------------------------------------------------------------------------
void ParticleSystem::Update()
{
	int64_t msc = m_clock.newFrame();
	GenerateParticles(msc);
	for (auto& prt : m_particles)
		prt.Update(static_cast<float>(msc));

	std::sort(m_particles.begin(), m_particles.end(),
			  [](Particle& prt1, Particle& prt2)
			{ return prt1.isAlive() > prt2.isAlive(); });
}
