#ifndef TONEMAPPING_INC
#define TONEMAPPING_INC

// GT tonemap
float uchimuraTonemapper(float x, float P, float a, float m, float l, float c, float b) {
    // Uchimura 2017, "HDR theory and practice"
    // Math: https://www.desmos.com/calculator/gslcdxvipg
    // Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;

    float w0 = 1.0 - smoothstep(0.0, m, x);
    float w2 = step(m + l0, x);
    float w1 = 1.0 - w0 - w2;

    float T = m * pow(x / m, c) + b;
    float S = P - (P - S1) * exp(CP * (x - S0));
    float L = m + a * (x - m);

    return T * w0 + L * w1 + S * w2;
}

vec3 uchimuraTonemapper(vec3 x, float P, float a, float m, float l, float c, float b) {
    // Uchimura 2017, "HDR theory and practice"
    // Math: https://www.desmos.com/calculator/gslcdxvipg
    // Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;

    vec3 w0 = 1.0 - smoothstep(0.0, m, x);
    vec3 w2 = step(m + l0, x);
    vec3 w1 = 1.0 - w0 - w2;

    vec3 T = m * pow(x / m, vec3(c)) + b;
    vec3 S = P - (P - S1) * exp(CP * (x - S0));
    vec3 L = m + a * (x - m);

    return T * w0 + L * w1 + S * w2;
}

vec3 uchimura(vec3 x) {
  const float P = 1.0;  // max display brightness
  const float a = 1.0;  // contrast
  const float m = 0.22; // linear section start
  const float l = 0.4;  // linear section length
  const float c = 1.33; // black
  const float b = 0.0;  // pedestal

  return uchimuraTonemapper(x, P, a, m, l, c, b);
}

//simple fit from: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilmApproximate(vec3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return clamp( ( x * ( a * x + b ) ) / ( x * ( c * x + d ) + e ), 0, 1);
}

vec3 ACESFilmRec2020( vec3 x )
{
    const float a = 15.8f;
    const float b = 2.12f;
    const float c = 1.2f;
    const float d = 5.92f;
    const float e = 1.9f;
    return ( x * ( a * x + b ) ) / ( x * ( c * x + d ) + e );
}

//code from: https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
//licensed under MIT license
const mat3 ACESInputMat =
{
    {0.59719, 0.35458, 0.04823},
    {0.07600, 0.90834, 0.01566},
    {0.02840, 0.13383, 0.83777}
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
const mat3 ACESOutputMat =
{
    { 1.60475, -0.53108, -0.07367},
    {-0.10208,  1.10813, -0.00605},
    {-0.00327, -0.07276,  1.07602}
};

vec3 RRTAndODTFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

//matrices are transposed because they are from HLSL code
vec3 ACESFitted(vec3 color)
{
    //color = transpose(ACESInputMat) * color;
    color = color * ACESInputMat;

    // Apply RRT and ODT
    color = RRTAndODTFit(color);
    //color = transpose(ACESOutputMat) * color;
    color = color * ACESOutputMat;
    color = clamp(color, 0, 1);
    return color;
}

vec3 tonemap_Uncharted2_internal(vec3 x)
{
	const float A = 0.15;
	const float B = 0.50;
	const float C = 0.10;
	const float D = 0.20;
	const float E = 0.02;
	const float F = 0.30;
    
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 tonemap_Uncharted2(vec3 x)
{
	const float W = 11.2;
    return tonemap_Uncharted2_internal(x * 2.0) / tonemap_Uncharted2_internal(vec3(W));
}

vec3 tonemap_filmic(vec3 x) {
  vec3 X = max(vec3(0.0), x - 0.004);
  vec3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
  return pow(result, vec3(2.2));
}

float tonemap_filmic(float x) {
  float X = max(0.0, x - 0.004);
  float result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
  return pow(result, 2.2);
}

#endif // #ifndef TONEMAPPING_INC