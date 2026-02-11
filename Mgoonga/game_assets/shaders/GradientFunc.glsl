// Evaluate cubic Bezier curve at t
vec2 cubicBezier(vec2 a, vec2 b, vec2 c, vec2 d, float t)
{

    float u = 1.0 - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;
    return (uuu * a) + (3.0 * uu * t * b) + (3.0 * u * tt * c) + (ttt * d);
}

// Find t such that bezier(t).x ≈ xTarget
float findTForX(float xTarget, vec2 p1, vec2 p2)
{
    float t0 = 0.0;
    float t1 = 1.0;
    float t = 0.5;
    for (int i = 0; i < 20; ++i) {
        t = 0.5 * (t0 + t1);
        float x = cubicBezier(p0, p1, p2, p3, t).x;
        if (abs(x - xTarget) < 1e-4) break;
        if (x < xTarget) t0 = t;
        else t1 = t;
    }
    return t;
}

// Rounded corner mask with aspect ratio correction
float cornerMask(vec2 uv, float radius, float aspect)
{
 // Convert uv to [-1, 1] range and apply aspect correction
    vec2 centered = uv * 2.0 - 1.0;
    centered.x *= aspect;

    // Get the vector to the nearest corner (±1, ±1)
    vec2 corner = vec2(1.0 * aspect, 1.0);
    vec2 dist = abs(centered) - (corner - radius);

    // Clamp negative values to zero (inside the rounded rect)
    float outsideDist = length(max(dist, 0.0));

    // Smooth transition at radius boundary
    return 1.0 - smoothstep(0.0, radius, outsideDist);
}


