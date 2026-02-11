#include "Bezier.h"
#include "calculus.h"

//------------------------------------------------------------
dbb::Bezier dbb::MakeCubic2DBezier(float _x1, float _y1, float _x2, float _y2)
{
  dbb::Bezier bezier;
  bezier.p0 = { 0.,0.,0.};
  bezier.p1 = { _x1, _y1,0. };
  bezier.p2 = { _x2, _y2,0. };
  bezier.p3 = { 1.,1.,0. };
  return bezier;
}

//---------------------------------------------------------
bool dbb::Equal(const dbb::Bezier& _first, const dbb::Bezier& _second)
{
  if (_first.p0 != _second.p0)
    return false;
  if (_first.p1 != _second.p1)
    return false;
  if (_first.p2 != _second.p2)
    return false;
  if (_first.p3 != _second.p3)
    return false;

  return true;
}

//---------------------------------------------------------
glm::vec3 dbb::GetPoint(const Bezier& _bezier, float u)
{
  /*float u = 1.0f - t;
  return u * u * u * bezier.p0 +
    3.0f * u * u * t * bezier.p1 +
    3.0f * u * t * t * bezier.p2 +
    t * t * t * bezier.p3;*/

  float u1 = (1.0f - u);
  float u2 = u * u;

  float b3 = u2 * u;
  float b2 = 3.0f * u2 * u1;
  float b1 = 3.0f * u * u1 * u1;
  float b0 = u1 * u1 * u1;

  return _bezier.p0 * b0 + _bezier.p1 * b1 + _bezier.p2 * b2 + _bezier.p3 * b3;
}

//----------------------------------------------------------------------------------
glm::vec3 DLL_MATH dbb::GetVelocity(const Bezier& bezier, float t)
{
  glm::vec3 term1 = 3.0f * (1 - t) * (1 - t) * (bezier.p1 - bezier.p0);
  glm::vec3 term2 = 6.0f * (1 - t) * t * (bezier.p2 - bezier.p1);
  glm::vec3 term3 = 3.0f * t * t * (bezier.p3 - bezier.p2);
  return term1 + term2 + term3;
}

//--------------------------------------------------------------------------------
glm::vec3 DLL_MATH dbb::GetAcceleration(const Bezier& bezier, float t)
{
  float u = 1.0f - t;
  return (6.0f * u * bezier.p0) -
    (12.0f * (1.0f - 2.0f * t) * bezier.p1) +
    (6.0f * (1.0f + 2.0f * t) * bezier.p2) -
    (6.0f * t * bezier.p3);
}

//--------------------------------------------------------------------------------
float dbb::GetArcLength(const Bezier& bezier, int subdivisions)
{
  auto velocityMagnitude = [&](float t) -> float
  {
    glm::vec3 velocity = GetVelocity(bezier, t);
    return glm::length(velocity);
  };
  return integrate(velocityMagnitude, 0.0f, 1.0f, subdivisions);
}

//--------------------------------------------------------------------------------
glm::vec3 dbb::GetPointAtDistance(const Bezier& _bezier, float _distance)
{
  //Arclength parametrization
  float arcLength = GetArcLength(_bezier);
  float t = _distance / arcLength;
  return GetPoint(_bezier, t);
}

//--------------------------------------------------------------------------------
std::vector<dbb::DistanceLUTEntry> dbb::CreateCumulativeDistanceLUT(const Bezier& bezier, int subdivisions)
{
  std::vector<dbb::DistanceLUTEntry> lut;
  lut.reserve(subdivisions + 1);

  glm::vec3 prevPoint = bezier.p0;
  float cumulativeDistance = 0.0f;
  lut.push_back({ 0.0f, cumulativeDistance }); // Add the first point

  for (int i = 1; i <= subdivisions; ++i)
  {
    float t = static_cast<float>(i) / subdivisions;
    glm::vec3 point = GetPoint(bezier, t);
    cumulativeDistance += glm::length(point - prevPoint);
    lut.push_back({ t, cumulativeDistance });
    prevPoint = point;
  }

  return lut;
}

//------------------------------------------------------------------------------------------------------------
glm::vec3 dbb::GetPointByDistance(const Bezier& bezier, float distance, const std::vector<DistanceLUTEntry>& lut)
{
  // Binary search to find the two closest entries in the LUT for the given distance
  auto it = std::lower_bound(lut.begin(), lut.end(), distance, [](const DistanceLUTEntry& entry, float dist) {
    return entry.cumulativeDistance < dist;
    });

  if (it == lut.end()) {
    return GetPoint(bezier, 1.0f); // Return the endpoint if the distance exceeds the arc length
  }
  if (it == lut.begin()) {
    return GetPoint(bezier, 0.0f); // Return the starting point if the distance is very small
  }

  // Interpolate between the two LUT entries
  const DistanceLUTEntry& prev = *(it - 1);
  const DistanceLUTEntry& next = *it;

  float t = prev.t + (next.t - prev.t) * ((distance - prev.cumulativeDistance) / (next.cumulativeDistance - prev.cumulativeDistance));

  return GetPoint(bezier, t);
}

//-------------------------------------------------------------------------------------
std::vector<glm::vec3> dbb::SolveBezier(const Bezier& bezier, float x)
{
  std::vector<glm::vec3> solutions;

  // Coefficients of the cubic polynomial for x(t)
  float a = -bezier.p0.x + 3.0f * bezier.p1.x - 3.0f * bezier.p2.x + bezier.p3.x;
  float b = 3.0f * (bezier.p0.x - 2.0f * bezier.p1.x + bezier.p2.x);
  float c = 3.0f * (-bezier.p0.x + bezier.p1.x);
  float d = bezier.p0.x - x;

  // Solve the cubic equation a*t^3 + b*t^2 + c*t + d = 0
  std::vector<float> ts = solveCubic(a, b, c, d);

  // Filter roots to only include those in the [0, 1] range
  ts.erase(std::remove_if(ts.begin(), ts.end(), [](float t) {
    return t < 0.0f || t > 1.0f;
    }), ts.end());

  // Evaluate the Bezier curve at each valid t to find the corresponding y values
  for (float t : ts)
  {
    glm::vec3 point = GetPoint(bezier, t);
    solutions.push_back(point);
  }

  return solutions;
}

//--------------------------------------------------------------------------------
void dbb::GetBoundingBox(const Bezier& bezier, glm::vec3& min, glm::vec3& max)
{
  min = glm::min(bezier.p0, bezier.p3);
  max = glm::max(bezier.p0, bezier.p3);

  // Check the derivative roots for x, y, z
  for (int i = 0; i < 3; i++)
  {
    float p0 = bezier.p0[i];
    float p1 = bezier.p1[i];
    float p2 = bezier.p2[i];
    float p3 = bezier.p3[i];

    float a = -p0 + 3.0f * p1 - 3.0f * p2 + p3;
    float b = 2.0f * (p0 - 2.0f * p1 + p2);
    float c = -p0 + p1;

    auto roots = FindQuadraticRoots(a, b, c);
    for (float t : roots)
    {
      if (t >= 0.0f && t <= 1.0f)
      {
        glm::vec3 point = GetPoint(bezier, t);
        min = glm::min(min, point);
        max = glm::max(max, point);
      }
    }
  }
}

//--------------------------------------------------------------------------------
glm::vec3 dbb::ClosestPointOnBezier(const Bezier& bezier, const glm::vec3& point, float tMin, float tMax, int depth)
{
  // If we've reached the maximum depth, return the midpoint
  if (depth == 0)
  {
    float tMid = (tMin + tMax) / 2.0f;
    return GetPoint(bezier, tMid);
  }

  // Split the interval into three parts
  float t1 = tMin + (tMax - tMin) / 3.0f;
  float t2 = tMin + 2.0f * (tMax - tMin) / 3.0f;

  // Evaluate the curve at t1 and t2
  glm::vec3 p1 = GetPoint(bezier, t1);
  glm::vec3 p2 = GetPoint(bezier, t2);

  // Compute the squared distances from the point to p1 and p2
  float d1 = squaredDistance(point, p1);
  float d2 = squaredDistance(point, p2);

  // Recursively search the interval with the smaller distance
  if (d1 < d2)
    return ClosestPointOnBezier(bezier, point, tMin, t2, depth - 1);
  else
    return ClosestPointOnBezier(bezier, point, t1, tMax, depth - 1);
}

//--------------------------------------------------------------------------------
void dbb::ConnectWithC1(const dbb::Bezier& b1, dbb::Bezier& b2)
{
  // Ensure position continuity
  b2.p0 = b1.p3;

  // Tangent at end of b1
  glm::vec3 tangent = b1.p3 - b1.p2;

  // Set b2.p1 to match the tangent
  b2.p1 = b2.p0 + tangent;
}

// Adjusts b1.p3 and b2.p0 based on b2.p1 to ensure C1 continuity
//-----------------------------------------------------------------------
void dbb::ConnectBackwardsWithC1(dbb::Bezier& b1, dbb::Bezier& b2)
{
  // Compute the desired tangent direction from b2
  glm::vec3 tangent = glm::normalize(b2.p1 - b1.p3); // start from old guess

  // Compute length of b1's outgoing handle
  float length = glm::length(b1.p3 - b1.p2);

  // Recompute p3 for b1 based on b2.p1
  b1.p3 = b1.p2 + glm::normalize(b2.p1 - b1.p2) * length;

  // Now assign b2.p0 to match b1.p3
  b2.p0 = b1.p3;
}
