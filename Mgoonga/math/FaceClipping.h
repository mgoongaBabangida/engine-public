#pragma once
#include "math.h"
#include "Geometry.h"
#include <glm/glm/gtx/norm.hpp>

// Sutherland - Hodgman algorithm
namespace dbb
{
  //--------------------------------------------------------------
  struct ReferenceFace
  {
    glm::vec3 normal;      // Normal of the reference face
    glm::vec3 tangent;     // Tangent vector (1st edge direction)
    glm::vec3 bitangent;   // Bitangent vector (2nd edge direction)
    glm::vec3 origin;      // Center of the reference face
  };

  //--------------------------------------------------------------
  struct Face
  {
    glm::vec3 normal;
    glm::vec3 vertices[4]; // Quad face
  };

  //--------------------------------------------------------------
  ReferenceFace GetReferenceFace(const OBB& box, const glm::vec3& axis)
  {
    // Find best local axis (one of the box's 3 face normals)
    float maxDot = -FLT_MAX;
    int bestAxis = -1;

    for (int i = 0; i < 3; ++i)
    {
      glm::vec3 localNormal = box.orientation[i];
      float d = glm::dot(axis, localNormal);
      if (d > maxDot) {
        maxDot = d;
        bestAxis = i;
      }
    }

    glm::vec3 normal = box.orientation[bestAxis];
    glm::vec3 tangent = box.orientation[(bestAxis + 1) % 3];
    glm::vec3 bitangent = box.orientation[(bestAxis + 2) % 3];

    glm::vec3 origin = box.origin + normal * box.size[bestAxis];

    return { normal, tangent, bitangent, origin };
  }

  //--------------------------------------------------------------
  ReferenceFace GetIncidentFace(const OBB& box, const glm::vec3& refNormal)
  {
    float minDot = FLT_MAX;
    int bestAxis = -1;

    for (int i = 0; i < 3; ++i)
    {
      glm::vec3 localNormal = box.orientation[i];
      float d = glm::dot(refNormal, localNormal);
      if (d < minDot) {
        minDot = d;
        bestAxis = i;
      }
    }

    glm::vec3 normal = box.orientation[bestAxis];
    glm::vec3 tangent = box.orientation[(bestAxis + 1) % 3];
    glm::vec3 bitangent = box.orientation[(bestAxis + 2) % 3];

    glm::vec3 origin = box.origin - normal * box.size[bestAxis]; // opposite face

    return { -normal, tangent, bitangent, origin };
  }

  //--------------------------------------------------------------
  Face BuildFacePolygon(const ReferenceFace& face, const glm::vec3& halfWidths)
  {
    glm::vec3 halfT = face.tangent * halfWidths.x;
    glm::vec3 halfB = face.bitangent * halfWidths.y;

    Face result;
    result.normal = face.normal;

    result.vertices[0] = face.origin - halfT - halfB;
    result.vertices[1] = face.origin + halfT - halfB;
    result.vertices[2] = face.origin + halfT + halfB;
    result.vertices[3] = face.origin - halfT + halfB;

    return result;
  }

  // Returns signed distance from point to plane
  //---------------------------------------------------------------------
  float PlaneDistance(const glm::vec3& point, const glm::vec3& normal, const glm::vec3& pointOnPlane) {
    return glm::dot(normal, point - pointOnPlane);
  }

  // Clip a polygon against a single plane
  //---------------------------------------------------------------------
  std::vector<glm::vec3> ClipPolygon(const std::vector<glm::vec3>& polygon,const glm::vec3& planeNormal,const glm::vec3& pointOnPlane)
  {
    std::vector<glm::vec3> result;
    size_t size = polygon.size();

    for (size_t i = 0; i < size; ++i)
    {
      const glm::vec3& A = polygon[i];
      const glm::vec3& B = polygon[(i + 1) % size];

      float da = PlaneDistance(A, planeNormal, pointOnPlane);
      float db = PlaneDistance(B, planeNormal, pointOnPlane);

      if (da >= 0) result.push_back(A); // A is inside
      if (da * db < 0.0f)
      {                  // Edge crosses the plane
        float t = da / (da - db);
        result.push_back(A + t * (B - A));
      }
    }
    return result;
  }

  //---------------------------------------------------------------------------------------
  std::vector<glm::vec3> ClipFaceToFace(const Face& refFace, const Face& incFace, float contactEpsilon = 0.0001f)
  {
    std::vector<glm::vec3> clipped(incFace.vertices, incFace.vertices + 4);

    // Build reference face edges & plane normals
    for (int i = 0; i < 4; ++i)
    {
      glm::vec3 v1 = refFace.vertices[i];
      glm::vec3 v2 = refFace.vertices[(i + 1) % 4];
      glm::vec3 edge = v2 - v1;
      glm::vec3 edgeNormal = glm::normalize(glm::cross(refFace.normal, edge)); // outward normal

      clipped = ClipPolygon(clipped, edgeNormal, v1);
      if (clipped.empty()) break; // No contact
    }

    // Optional: remove near-duplicate points
    for (int i = (int)clipped.size() - 1; i >= 0; --i)
    {
      for (int j = i - 1; j >= 0; --j)
      {
        if (glm::length2(clipped[i] - clipped[j]) < contactEpsilon)
        {
          clipped.erase(clipped.begin() + i);
          break;
        }
      }
    }
    return clipped;
  }
}
