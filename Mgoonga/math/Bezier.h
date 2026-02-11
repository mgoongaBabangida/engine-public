#pragma once

#include "math.h"

#include <glm\glm\glm.hpp>
#include <array>

namespace dbb
{
  struct Bezier
  {
    glm::vec3 p0;
    glm::vec3 p1;
    glm::vec3 p2;
    glm::vec3 p3;
  };

  Bezier DLL_MATH MakeCubic2DBezier(float, float, float, float);

  bool DLL_MATH Equal(const Bezier&, const Bezier&);

  // Structure to hold t and its corresponding cumulative distance
  struct DistanceLUTEntry
  {
    float t;
    float cumulativeDistance;
  };

  // Function to get a point on Bezier at a given t
  glm::vec3 DLL_MATH GetPoint(const Bezier& _bezier, float _t); // t is [0 - 1]

  // Derivative of cubic bezier is cuadratic bezier (velocity of the point) (get Tangent and Normal), useful for generating roads in 3d
  // Function to calculate the velocity at a given t
  glm::vec3 DLL_MATH GetVelocity(const Bezier& bezier, float t); // first derivative

    // Second derivative is acceleration of the point (linear bezier curve or just line segment)
  // Function to calculate the acceleration at a given t
  glm::vec3  DLL_MATH GetAcceleration(const Bezier& bezier, float t); // second derivative

  // Third derivative is Jerk is rate of change of acceleration(this is a point)
  
  float DLL_MATH GetArcLength(const Bezier& bezier, int subdivisions = 100);

  glm::vec3 DLL_MATH GetPointAtDistance(const Bezier& _bezier, float _distance);

  //Curvature Det(P1,P2)/(||P1||*||P1||*||P1||); how bent it is at a given point

  // Function to create the Cumulative Distance LUT
  std::vector<DistanceLUTEntry> DLL_MATH CreateCumulativeDistanceLUT(const Bezier& bezier, int subdivisions = 100);

  // Function to evaluate a point on the Bezier curve based on its distance using the LUT
  glm::vec3 DLL_MATH GetPointByDistance(const Bezier& bezier, float distance, const std::vector<DistanceLUTEntry>& lut);

  // Function to solve for y given x on a 2D cubic Bezier curve
  std::vector<glm::vec3> SolveBezier(const Bezier& bezier, float x);

  //With derivateve can get bounding box of bezier curve
  // Function to find the bounding box of the cubic Bezier curve
  void GetBoundingBox(const Bezier& bezier, glm::vec3& min, glm::vec3& max);

  // Recursive function to find the closest point on the Bezier curve to the given point
  glm::vec3 DLL_MATH ClosestPointOnBezier(const Bezier& bezier, const glm::vec3& point, float tMin = 0.0f, float tMax = 1.0f, int depth = 10);

  // Adjusts b2.p1 to ensure C1 continuity with b1
  void DLL_MATH ConnectWithC1(const Bezier& b1, Bezier& b2);

  // Adjusts b1.p3 and b2.p0 based on b2.p1 to ensure C1 continuity
  void DLL_MATH ConnectBackwardsWithC1(dbb::Bezier& b1, dbb::Bezier& b2);
}
