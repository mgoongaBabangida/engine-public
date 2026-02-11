const float LUT_SIZE  = 64.0; // ltc_texture size
const float LUT_SCALE = (LUT_SIZE - 1.0)/LUT_SIZE;
const float LUT_BIAS  = 0.5/LUT_SIZE;

const float gamma = 2.2;

vec3 PowVec3(vec3 v, float p) { return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p)); }
vec3 ToLinear(vec3 v) { return PowVec3(v, gamma); }

//area lights functions
// Vector form without project to the plane (dot with the normal)
// Use for proxy sphere clipping
vec3 IntegrateEdgeVec(vec3 v1, vec3 v2);
float IntegrateEdge(vec3 v1, vec3 v2);
// P is fragPos in world space (LTC distribution)
vec3 LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided);
// PBR-maps for roughness (and metallic) are usually stored in non-linear
// color space (sRGB), so we use these functions to convert into linear RGB.

vec3 ToSRGB(vec3 v)   { return PowVec3(v, 1.0/gamma); }

vec3 IntegrateEdgeVec(vec3 v1, vec3 v2)
{
    // Using built-in acos() function will result flaws
    // Using fitting result for calculating acos()
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206*y)*y;
    float b = 3.4175940 + (4.1616724 + y)*y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - v;

    return cross(v1, v2)*theta_sintheta;
}

float IntegrateEdge(vec3 v1, vec3 v2)
{
    return IntegrateEdgeVec(v1, v2).z;
}

vec3 LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided)
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    Minv = Minv * transpose(mat3(T1, T2, N));

    // polygon (allocate 4 vertices for clipping)
    vec3 L[4];
    // transform polygon from LTC back to origin Do (cosine weighted)
    L[0] = Minv * (points[0].xyz - P);
    L[1] = Minv * (points[1].xyz - P);
    L[2] = Minv * (points[2].xyz - P);
    L[3] = Minv * (points[3].xyz - P);

    // use tabulated horizon-clipped sphere
    // check if the shading point is behind the light
    vec3 dir = points[0].xyz - P; // LTC space
    vec3 lightNormal = cross(points[1].xyz - points[0].xyz, points[3].xyz - points[0].xyz);
    bool behind = (dot(dir, lightNormal) < 0.0);

    // cos weighted space
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    // integrate
    vec3 vsum = vec3(0.0);
    vsum += IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);

    // form factor of the polygon in direction vsum
    float len = length(vsum);

    float z = vsum.z/len;
    if (behind)
        z = -z;

    vec2 uv = vec2(z*0.5f + 0.5f, len); // range [0, 1]
    uv = uv*LUT_SCALE + LUT_BIAS;

    // Fetch the form factor for horizon clipping
    float scale = texture(LTC2, uv).w;

    float sum = len*scale;
    if (!behind && !twoSided)
        sum = 0.0;

    // Outgoing radiance (solid angle) for the entire polygon
    vec3 Lo_i = vec3(sum, sum, sum);
    return Lo_i;
}

vec4 CalculateAreaLight(AreaLight _areaLight, vec4 _areaLightTranslate, vec3 _albedo, vec3 _normal, float _roughness)
{		
    // gamma correction
    //vec3 mDiffuse = texture(material.diffuse, Texcoord).xyz;// * vec3(0.7f, 0.8f, 0.96f);
	
	vec3 mDiffuse = _albedo;
    vec3 mSpecular = ToLinear(vec3(0.23f, 0.23f, 0.23f)); // mDiffuse
    vec3 result = vec3(0.0f);

    vec3 N = normalize(_normal);
    vec3 V = normalize(camPos.xyz - thePosition);
    vec3 P = thePosition;
    float dotNV = clamp(dot(N, V), 0.0f, 1.0f);

    // use roughness and sqrt(1-cos_theta) to sample M_texture
    vec2 uv = vec2(clamp(_roughness, 0.01f, 1.0f), sqrt(1.0f - dotNV));
    uv = uv*LUT_SCALE + LUT_BIAS;

	uv.x = clamp(uv.x, 0.0f, 1.0f);
	uv.y = clamp(uv.y, 0.0f, 1.0f);

    // get 4 parameters for inverse_M
    vec4 t1 = texture(LTC1, uv);

    // Get 2 parameters for Fresnel calculation
    vec4 t2 = texture(LTC2, uv);

    mat3 Minv = mat3(
        vec3(t1.x, 0, t1.y),
        vec3(  0,  1,    0),
        vec3(t1.z, 0, t1.w)
    );

    // translate light source for testing
    vec3 translatedPoints[4];
    translatedPoints[0] = _areaLight.points[0].xyz + _areaLightTranslate.xyz;
    translatedPoints[1] = _areaLight.points[1].xyz + _areaLightTranslate.xyz;
    translatedPoints[2] = _areaLight.points[2].xyz + _areaLightTranslate.xyz;
    translatedPoints[3] = _areaLight.points[3].xyz + _areaLightTranslate.xyz;

    // Evaluate LTC shading
    vec3 diffuse = LTC_Evaluate(N, V, P, mat3(1), translatedPoints, _areaLight.twoSided);
    vec3 specular = LTC_Evaluate(N, V, P, Minv, translatedPoints, _areaLight.twoSided);

    // GGX BRDF shadowing and Fresnel
    // t2.x: shadowedF90 (F90 normally it should be 1.0)
    // t2.y: Smith function for Geometric Attenuation Term, it is dot(V or L, H).
    specular *= mSpecular*t2.x + (1.0f - mSpecular) * t2.y;

    result = _areaLight.color.xyz * _areaLight.intensity * (specular + mDiffuse * diffuse);

    return vec4(ToSRGB(result), 1.0f);
}
