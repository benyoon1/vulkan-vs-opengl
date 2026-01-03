#version 330 core
out vec4 FragColor;

uniform mat4 u_inverseProjection;
uniform mat4 u_inverseView;
uniform vec2 u_screenSize;
uniform vec3 u_sunDirection;

// --- UTILITY FUNCTIONS ---
float random(vec3 p) {
    p = fract(p * vec3(443.8975, 441.4234, 437.1956));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}
float noise(vec3 st) {
    vec3 i = floor(st);
    vec3 f = fract(st);
    float a = random(i);
    float b = random(i + vec3(1.0, 0.0, 0.0));
    float c = random(i + vec3(0.0, 1.0, 0.0));
    float d = random(i + vec3(1.0, 1.0, 0.0));
    float e = random(i + vec3(0.0, 0.0, 1.0));
    float f_ = random(i + vec3(1.0, 0.0, 1.0));
    float g = random(i + vec3(0.0, 1.0, 1.0));
    float h = random(i + vec3(1.0, 1.0, 1.0));
    vec3 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) +
    (c - a) * u.y * (1.0 - u.x) +
    (d - b) * u.y * u.x +
    (e - a) * u.z * (1.0 - u.y) * (1.0 - u.x) +
    (f_ - b) * u.z * (1.0 - u.y) * u.x +
    (g - c) * u.z * u.y * (1.0 - u.x) +
    (h - d) * u.z * u.y * u.x;
}
float fbm(vec3 p) {
    float f = 0.0;
    f += 0.5 * noise(p);
    p *= 2.02;
    f += 0.25 * noise(p);
    p *= 2.03;
    f += 0.125 * noise(p);
    p *= 2.01;
    f += 0.0625 * noise(p);
    return f / 0.9375;
}

void main() {
    // Stable view direction calculation
    vec2 ndc = gl_FragCoord.xy / u_screenSize * 2.0 - 1.0;
    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 viewSpace = u_inverseProjection * clip;
    viewSpace.xyz /= viewSpace.w;
    vec3 worldDir = (u_inverseView * vec4(viewSpace.xyz, 0.0)).xyz;
    vec3 viewDir = normalize(worldDir);

    // Day/Night Factors
    float sunDot = max(0.0, dot(viewDir, u_sunDirection));
    float nightFactor = 1.0 - smoothstep(-0.1, 0.1, u_sunDirection.y);
    float sunsetFactor = smoothstep(0.0, 0.08, u_sunDirection.y) * smoothstep(0.15, 0.05, u_sunDirection.y);

    vec3 daySkyColor = vec3(0.3, 0.6, 0.95);
    vec3 nightSkyColor = vec3(0.006, 0.012, 0.024);
    vec3 horizonColor = vec3(0.9, 0.75, 0.5);
    float horizonGlow = pow(1.0 - max(0.0, viewDir.y), 5.0);
    vec3 skyColor = mix(daySkyColor, horizonColor, horizonGlow);
    vec3 deepRedSunsetColor = vec3(0.9, 0.25, 0.0);
    skyColor = mix(skyColor, deepRedSunsetColor, sunsetFactor * 0.7);
    skyColor = mix(skyColor, nightSkyColor, nightFactor);
    vec3 middaySunColor = vec3(1.0, 0.95, 0.85);
    vec3 settingSunColor = vec3(1.0, 0.6, 0.2);
    vec3 dynamicSunColor = mix(middaySunColor, settingSunColor, sunsetFactor * 1.5);
    float sunHaze = pow(sunDot, 16.0);
    float sunGlow = pow(sunDot, 256.0);
    float sunCore = pow(sunDot, 1024.0);
    float sunPhotosphere = pow(sunDot, 4096.0);
    vec3 sunColor = (sunHaze * 0.4 + sunGlow * 0.6 + sunCore * 1.2 + sunPhotosphere * 0.5) * dynamicSunColor;
    sunColor *= 1.0 - nightFactor;

    vec3 finalColor = skyColor + sunColor;
    finalColor = finalColor / (finalColor + vec3(1.0));
    finalColor = pow(finalColor, vec3(1.0 / 2.2));
    FragColor = vec4(finalColor, 1.0);
}
