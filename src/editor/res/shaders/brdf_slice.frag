in vec2 v_texcoord;

const float m_pi   = 3.1415926535897932384626434;
const float m_i_pi = 0.3183098861837906715377675;

// https://www.shadertoy.com/view/NtlyWX

float cos_theta  (vec3 w) {return w.z; }
float cos_2_theta(vec3 w) {return w.z * w.z; }
float sin_2_theta(vec3 w) {return max(0.0, 1.0 - cos_2_theta(w)); }
float sin_theta  (vec3 w) {return sqrt(sin_2_theta(w)); }
float tan_theta  (vec3 w) {return sin_theta(w) / cos_theta(w); }
float cos_phi    (vec3 w) {return (sin_theta(w) == 0.0) ? 1.0 : clamp(w.x / sin_theta(w), -1.0, 1.0); }
float sin_phi    (vec3 w) {return (sin_theta(w) == 0.0) ? 0.0 : clamp(w.y / sin_theta(w), -1.0, 1.0); }
float cos_2_phi  (vec3 w) {return cos_phi(w) * cos_phi(w); }
float sin_2_phi  (vec3 w) {return sin_phi(w) * sin_phi(w); }

float p22_ggx_isotropic(float x, float y, float alpha)
{
    float x_sqr     = x * x;
    float y_sqr     = y * y;
    float alpha_sqr = alpha * alpha;
    float denom     = 1.0 + (x_sqr / alpha_sqr) + (y_sqr / alpha_sqr);
    return 1.0 / ((m_pi * alpha_sqr) * (denom * denom));
}

float p22_ggx_anisotropic(float x, float y, float alpha_x, float alpha_y)
{
    float x_sqr       = x * x;
    float y_sqr       = y * y;
    float alpha_x_sqr = alpha_x * alpha_x;
    float alpha_y_sqr = alpha_y * alpha_y;
    float denom       = 1.0 + (x_sqr / alpha_x_sqr) + (y_sqr / alpha_y_sqr);
    float denom_sqr   = denom * denom;
    return 1.0 / ((m_pi * alpha_x * alpha_y) * denom_sqr);
}

float slope_ndf_ggx_isotropic(vec3 omega_h, float alpha)

{
    float slope_x     = -(omega_h.x / omega_h.z);
    float slope_y     = -(omega_h.y / omega_h.z);
    float cos_theta   = cos_theta(omega_h);
    float cos_2_theta = cos_theta * cos_theta;
    float cos_4_theta = cos_2_theta * cos_2_theta;
    float ggx_p22     = p22_ggx_isotropic(slope_x, slope_y, alpha);
    return ggx_p22 / cos_4_theta;
}

float slope_ndf_ggx_anisotropic(vec3 omega_h, float alpha_x, float alpha_y)
{
    float slope_x     = -(omega_h.x / omega_h.z);
    float slope_y     = -(omega_h.y / omega_h.z);
    float cos_theta   = cos_theta(omega_h);
    float cos_2_theta = cos_theta * cos_theta;
    float cos_4_theta = cos_2_theta * cos_2_theta;
    float ggx_p22     = p22_ggx_anisotropic(slope_x, slope_y, alpha_x, alpha_y);
    return ggx_p22 / cos_4_theta;
}

float lambda_ggx_isotropic(vec3 omega, float alpha)
{
    float a = 1.0 / (alpha * tan_theta(omega));
    return 0.5 * (-1.0 + sqrt(1.0 + 1.0 / (a * a)));
}

float lambda_ggx_anisotropic(vec3 omega, float alpha_x, float alpha_y)
{
    float cos_phi = cos_phi(omega);
    float sin_phi = sin_phi(omega);
    float alpha_o = sqrt(cos_phi * cos_phi * alpha_x * alpha_x + sin_phi * sin_phi * alpha_y * alpha_y);
    float a       = 1.0 / (alpha_o * tan_theta(omega));
    return 0.5 * (-1.0 + sqrt(1.0 + 1.0 / (a * a)));
}

float ggx_isotropic_ndf(float N_dot_H, float alpha)
{
    float a = N_dot_H * alpha;
    float k = alpha / (1.0 - N_dot_H * N_dot_H + a * a);
    return k * k * m_i_pi;
}

float ggx_anisotropic_ndf(
    float alpha_t,
    float alpha_b,
    float T_dot_H,
    float B_dot_H,
    float N_dot_H
)
{
    vec3 v = vec3(
        alpha_b * T_dot_H,
        alpha_t * B_dot_H,
        alpha_t * alpha_b * N_dot_H
    );
    float v2 = dot(v, v);
    float a2 = alpha_t * alpha_b;
    float w2 = a2 / v2;
    return a2 * w2 * w2 * m_i_pi;
}

float ggx_isotropic_visibility(float N_dot_V, float N_dot_L, float alpha)
{
    float a2 = alpha * alpha;
    float GV = N_dot_L * sqrt(N_dot_V * N_dot_V * (1.0 - a2) + a2);
    float GL = N_dot_V * sqrt(N_dot_L * N_dot_L * (1.0 - a2) + a2);
    return 0.5 / (GV + GL);
}

float ggx_anisotropic_visibility(
    float alpha_t,
    float alpha_b,
	float T_dot_V,
    float B_dot_V,
    float N_dot_V,
    float T_dot_L,
    float B_dot_L,
    float N_dot_L
)
{
    float lambda_V = N_dot_L * length(vec3(alpha_t * T_dot_V, alpha_b * B_dot_V, N_dot_V));
    float lambda_L = N_dot_V * length(vec3(alpha_t * T_dot_L, alpha_b * B_dot_L, N_dot_L));
    float v = 0.5 / (lambda_V + lambda_L);
    return clamp(v, 0.0, 1.0);
}

vec3 fresnel_schlick(float cos_theta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - cos_theta, 5.0);
}

float clamped_dot(vec3 x, vec3 y)
{
    return clamp(dot(x, y), 0.001, 1.0);
}

vec3 slope_brdf(
    vec3  base_color,
    float roughness_x,
    float roughness_y,
    float metalness,
    mat3  TBN_t,
    vec3  L,
    vec3  V,
    vec3  T,
    vec3  B,
    vec3  N
)
{
    float alpha_x   = roughness_x * roughness_x;
    float alpha_y   = roughness_y * roughness_y;
    vec3  wo        = normalize(TBN_t * V);
    vec3  wg        = normalize(TBN_t * N); // ( should be (0,0,1)^T )
    vec3  wi        = normalize(TBN_t * L);
    vec3  wh        = normalize(wo + wi);
    float wi_dot_wh = clamp(dot(wi, wh), 0.0, 1.0);
    float wg_dot_wi = clamp(cos_theta(wi), 0.0, 1.0);
    float lambda_wo = lambda_ggx_anisotropic(wo, alpha_x, alpha_y);
    float lambda_wi = lambda_ggx_anisotropic(wi, alpha_x, alpha_y);
    float D         = slope_ndf_ggx_anisotropic(wh, alpha_x, alpha_y);
    float G         = 1.0 / (1.0 + lambda_wo + lambda_wi);

    float specular_brdf = max(
        wg_dot_wi * (D * G) / (4.0 * cos_theta(wi) * cos_theta(wo)),
        0.0
    );

    vec3  H                 = normalize(L + V);
    float N_dot_L           = clamped_dot(N, L);
    float V_dot_H           = clamped_dot(V, H);
    vec3  diffuse_brdf      = N_dot_L * base_color * m_i_pi;
    float fresnel           = pow(1.0 - abs(V_dot_H), 5.0);
    vec3  conductor_fresnel = specular_brdf * (base_color + (1.0 - base_color) * fresnel);
    float f0                = 0.04;
    float fr                = f0 + (1.0 - f0) * fresnel;
    vec3  fresnel_mix       = mix(diffuse_brdf, vec3(specular_brdf), fr);
    return mix(fresnel_mix, conductor_fresnel, metalness);
}

vec3 brdf(
    vec3  base_color,
    float roughness_x,
    float roughness_y,
    float metallic,
    float reflectance,
    vec3  L,
    vec3  V,
    vec3  T,
    vec3  B,
    vec3  N
)
{
    float alpha_x = roughness_x * roughness_x;
    float alpha_y = roughness_y * roughness_y;
    vec3  F0      = 0.16 * reflectance * reflectance * (1.0 - metallic) + base_color * metallic;
    vec3  H       = normalize(L + V);
    float N_dot_H = clamped_dot(N, H);
    float N_dot_V = clamped_dot(N, V);
    float N_dot_L = clamped_dot(N, L);
    float T_dot_V = dot(T, V);
    float B_dot_V = dot(B, V);
    float T_dot_L = dot(T, L);
    float B_dot_L = dot(B, L);
    float T_dot_H = dot(T, H);
    float B_dot_H = dot(B, H);
    float D       = ggx_anisotropic_ndf       (alpha_x, alpha_y, T_dot_H, B_dot_H, N_dot_H);
    float Vis     = ggx_anisotropic_visibility(alpha_x, alpha_y, T_dot_V, B_dot_V, N_dot_V, T_dot_L, B_dot_L, N_dot_L);
    vec3  F                   = fresnel_schlick(max(dot(V, H), 0.0), F0);
    vec3  specular_microfacet = D * Vis * F;
    vec3  diffuse_lambert     = m_i_pi * (1.0 - metallic) * base_color;
    vec3  diffuse_factor      = vec3(1.0) - F;
    //return N_dot_L * (diffuse_factor * diffuse_lambert + specular_microfacet);
    return (diffuse_factor * diffuse_lambert + specular_microfacet);
}

float sample_light_visibility(vec4 position, uint light_index, float N_dot_L)
{
#if defined(ERHE_SHADOW_MAPS)

#if defined(ERHE_BINDLESS_TEXTURE)
    sampler2DArray s_shadow = sampler2DArray(light_block.shadow_texture);
#endif

    Light light = light_block.lights[light_index];
    vec4  position_in_light_texture_homogeneous = light.texture_from_world * position;

    vec4  position_in_light_texture = position_in_light_texture_homogeneous / position_in_light_texture_homogeneous.w;
    float depth_in_light_texture    = position_in_light_texture.z;
    float sampled_depth = texture(
        s_shadow,
        vec3(
            position_in_light_texture.xy,
            float(light_index)
        )
    ).x;

    float bias = 0.005 * sqrt(1.0 - N_dot_L * N_dot_L) / N_dot_L; // tan(acos(N_dot_L))
    bias = clamp(bias, 0.0, 0.01);
    if (camera.cameras[0].clip_depth_direction < 0.0) {
        if (depth_in_light_texture + bias >= sampled_depth) { // reverse depth
            return 1.0;
        }
    } else {
        if (depth_in_light_texture - bias <= sampled_depth) { // forward depth
            return 1.0;
        }
    }
    return 0.0;
#else
    return 1.0;
#endif
}

float Sqr(float x)
{
    return x * x;
}

vec3 rotate_vector(vec3 v, vec3 axis, float angle)
{
    vec3 n;
    axis = normalize(axis);
    n = axis * dot(axis, v);
    return n + cos(angle) * (v - n) + sin(angle) * cross(axis, v);
}

void main()
{
    Material material = material.materials[light_block.brdf_material];
    const int   useThetaHSquared = 0;
    const int   useNDotL         = 0;
    const int   showChroma       = 0;
    float phiD             = light_block.brdf_phi_incident_phi.x;
    float incidentPhi      = light_block.brdf_phi_incident_phi.y;
    const float brightness       = 1.0;
    const float exposure         = 0.0;
    const float gamma            = 2.2;

    // orthonormal vectors
    vec3 normal    = vec3(0, 0, 1);
    vec3 tangent   = vec3(1, 0, 0);
    vec3 bitangent = vec3(0, 1, 0);

    // thetaH and thetaD vary from [0 - pi/2]
    const float M_PI = 3.1415926535897932384626433832795;

    float thetaH = v_texcoord.r;

    if (useThetaHSquared != 0) {
        thetaH = Sqr(thetaH) / (M_PI * 0.5);
    }

    float thetaD = v_texcoord.g;

    // compute H from thetaH,phiH where (phiH = incidentPhi)
    float phiH      = incidentPhi;
    float sinThetaH = sin(thetaH);
    float cosThetaH = cos(thetaH);
    float sinPhiH   = sin(phiH);
    float cosPhiH   = cos(phiH);
    vec3 H = vec3(sinThetaH * cosPhiH, sinThetaH * sinPhiH, cosThetaH);

    // compute D from thetaD,phiD
    float sinThetaD = sin(thetaD);
    float cosThetaD = cos(thetaD);
    float sinPhiD   = sin(phiD);
    float cosPhiD   = cos(phiD);
    vec3  D         = vec3(sinThetaD * cosPhiD, sinThetaD * sinPhiD, cosThetaD);

    // compute L by rotating D into incident frame
    vec3 L = rotate_vector(
        rotate_vector(D, bitangent, thetaH),
        normal,
        phiH
    );

    // compute V by reflecting L across H
    vec3 V = 2 * dot(H, L) * H - L;
    vec3 b = brdf(
        material.base_color.rgb,
        material.roughness.x,
        material.roughness.y,
        material.metallic,
        material.reflectance,
        L,
        V,
        tangent,
        bitangent,
        normal
    );

    // apply N . L
    if (useNDotL != 0) {
        b *= clamp(L[2], 0, 1);
    }

    if (showChroma != 0) {
        float norm = max(b[0], max(b[1], b[2]));
        if (norm > 0) {
            b /= norm;
        }
    }

    // brightness
    b *= brightness;

    // exposure
    b *= pow(2.0, exposure);

    // gamma
    b = pow(b, vec3(1.0 / gamma));

    out_color = vec4(
        clamp(b, vec3(0.0), vec3(1.0)),
        1.0
    );
}
