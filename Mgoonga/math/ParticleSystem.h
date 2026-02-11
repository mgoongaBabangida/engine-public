#pragma once

#include <base/interfaces.h>
#include <math/Timer.h>
#include <math/Particle.h>
#include <math/Bezier.h>
#include <math/Transform.h>

#include <glm\glm\glm.hpp>
#include <glm\glm\gtc\matrix_transform.hpp>
#include <glm\glm\gtx\transform.hpp>

//----------------------------------------------------
class DLL_MATH ParticleSystem: public IParticleSystem
{
public:
	ParticleSystem(float						_pps,
								 float						_speed,
								 float						_gravityComplient,
								 float						_lifeLength,
								 glm::vec3				_systemCenter,
								 const Texture*		_texture,
								 ISound*					_sound,
								 size_t						_num_rows_in_texture,
								 float						_duration = 10'000.0f);
	
	ParticleSystem(const ParticleSystem&) = delete;
	ParticleSystem& operator=(const ParticleSystem&) = delete;
	virtual ~ParticleSystem();

	virtual void								              Start();
	virtual void								              GenerateParticles(int64_t _tick);
	virtual std::vector<Particle>::iterator		PrepareParticles(const glm::vec3& cameraPosition);
	virtual std::vector<Particle>&				    GetParticles()	{ return  m_particles; }
	virtual bool								              IsFinished();
	virtual bool								              IsStarted();
	virtual void								              Reset();

	virtual float&										ConeAngle() override					{ return m_cone_angle; }
	virtual float&										Speed() override							{ return m_speed; }
	virtual float&										RandomizeMagnitude() override { return m_randomize_magnitude; }
	virtual glm::vec3&								Scale() override							{ return m_scale; } // @todo should be individual property of a particle
	virtual float&										BaseRadius() override					{ return m_base_radius; }
	virtual float&										LifeLength() override					{ return m_life_length; }
	virtual int&											ParticlesPerSecond() override { return m_pps; }
	virtual bool&											Loop() override								{ return m_loop; }
	virtual float&										Gravity() override						{ return m_gravity_complient; }
	virtual IParticleSystem::Type&		SystemType() override					{ return m_type; }
	virtual glm::vec3&								GetSystemCenter()							{ return m_transform.getTranslationRef(); }
	virtual void											SetSizeBezier(std::array<glm::vec3, 4>);

	virtual const Texture* GetTexture() override { return m_texture; }
	virtual void SetTexture(Texture* _texture) override  { m_texture = _texture; }

	virtual ITransform* GetTransform() override { return &m_transform; }
	virtual std::vector<glm::vec3> GetExtremsWorldSpace() const override;

protected:
	virtual void									Update();

	void													_emitParticles();
	virtual	glm::vec3							_calculateParticles();

	std::vector<Particle>				m_particles;

	// parameters
	Transform			m_transform;
	float					m_base_radius = 0.0f;
	int						m_pps; //particles per second (rate over time in unity) // @todo add rate over distance and burst
	Type					m_type = CONE;
	float					m_cone_angle = PI / 6.28f;
	float					m_speed = 1.0f/ 50.0f;
	float					m_randomize_magnitude = 0.0f;
	float					m_gravity_complient = 0.0f;
	float					m_life_length;
	float					m_duration;
	bool					m_loop;
	glm::vec3			m_scale; // @todo size of the particle -> scale of its transform
	dbb::Bezier		m_particle_size_over_time;
	bool					m_use_size_over_time = false;

	//@todo it should have its transform, move system center there and controle direction of the cone
	//@ blending modes

	int32_t m_max_particles = 1'000;

	//helpers
	math::eClock									m_clock;
	std::unique_ptr<math::Timer>	m_timer;
	ISound*												m_sound;
	uint32_t											m_cur_particles = 0;
	size_t												m_num_rows_in_texture = 1;
	int64_t												m_time_wo_new_particles = 0;
	Texture*											m_texture = nullptr;
};
