#pragma once
#include <functional>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dbb
{
  // Trapezoidal Rule for numerical integration
  //------------------------------------------------------------------------------
  double integrate(std::function<double(double)> func, double a, double b, int n)
  {
    double h = (b - a) / n; // Width of each trapezoid
    double integral = 0.0;

    // Sum the areas of each trapezoid
    for (int i = 0; i <= n; ++i) {
      double x = a + i * h;
      if (i == 0 || i == n) {
        integral += 0.5 * func(x);
      }
      else {
        integral += func(x);
      }
    }

    integral *= h;
    return integral;
  }

  // Central difference method for numerical differentiation
  //------------------------------------------------------------------------------
  double differentiate(std::function<double(double)> func, double x, double h = 1e-5)
  {
    return (func(x + h) - func(x - h)) / (2 * h);
  }

  // Function to create a numerical derivative function
  //------------------------------------------------------------------------------
  std::function<double(double)> make_derivative(std::function<double(double)> func, double h = 1e-5)
  {
    return [func, h](double x)
    {
      return (func(x + h) - func(x - h)) / (2 * h);
    };
  }

  // Helper function to solve a cubic equation for real roots
  //------------------------------------------------------------------------------
  std::vector<float> solveCubic(float a, float b, float c, float d) 
  {
    std::vector<float> roots;

    if (a == 0.0f) { // Handle the case where it's actually a quadratic equation
      if (b == 0.0f) { // Handle the case where it's a linear equation
        if (c != 0.0f) {
          roots.push_back(-d / c);
        }
      }
      else {
        float discriminant = c * c - 4.0f * b * d;
        if (discriminant >= 0.0f) {
          float sqrtDiscriminant = sqrt(discriminant);
          roots.push_back((-c + sqrtDiscriminant) / (2.0f * b));
          roots.push_back((-c - sqrtDiscriminant) / (2.0f * b));
        }
      }
    }
    else {
      // Normalize coefficients
      b /= a;
      c /= a;
      d /= a;

      float p = (3.0f * c - b * b) / 3.0f;
      float q = (2.0f * b * b * b - 9.0f * b * c + 27.0f * d) / 27.0f;
      float discriminant = q * q / 4.0f + p * p * p / 27.0f;

      if (discriminant > 0.0f) { // One real root
        float u = cbrt(-q / 2.0f + sqrt(discriminant));
        float v = cbrt(-q / 2.0f - sqrt(discriminant));
        roots.push_back(u + v - b / 3.0f);
      }
      else if (discriminant == 0.0f) { // Two real roots
        float u = cbrt(-q / 2.0f);
        roots.push_back(2.0f * u - b / 3.0f);
        roots.push_back(-u - b / 3.0f);
      }
      else { // Three real roots
        float r = sqrt(-p / 3.0f);
        float phi = acos(-q / (2.0f * r * r * r));
        roots.push_back(2.0f * r * cos(phi / 3.0f) - b / 3.0f);
        roots.push_back(2.0f * r * cos((phi + 2.0f * M_PI) / 3.0f) - b / 3.0f);
        roots.push_back(2.0f * r * cos((phi + 4.0f * M_PI) / 3.0f) - b / 3.0f);
      }
    }
    return roots;
  }

  // Function to calculate the squared distance between two points
  //------------------------------------------------------------------------------
  float squaredDistance(const glm::vec3& p1, const glm::vec3& p2)
  {
    glm::vec3 diff = p1 - p2;
    return glm::dot(diff, diff);
  }

  // Function to find roots of a quadratic equation ax^2 + bx + c = 0
  //------------------------------------------------------------------------------
  std::array<float, 2> FindQuadraticRoots(float a, float b, float c)
  {
    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) return { -1.0f, -1.0f }; // No real roots
    float sqrtDisc = sqrt(discriminant);
    return { (-b + sqrtDisc) / (2.0f * a), (-b - sqrtDisc) / (2.0f * a) };
  }
}