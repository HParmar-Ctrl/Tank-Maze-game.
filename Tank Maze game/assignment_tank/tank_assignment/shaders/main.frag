#version 330

// ============================================================
//  Fragment Shader — Blinn-Phong reflection model
//  Supports both textured and solid-colour rendering via
//  the useTexture uniform flag.
// ============================================================

in vec3 fragPos;
in vec3 fragNormal;
in vec2 fragTexCoord;

// Lighting parameters
uniform vec3  lightPos;          // world-space light position
uniform vec3  lightColor;        // light colour (RGB)
uniform vec3  viewPos;           // world-space camera position

uniform float ambientStrength;   // ambient coefficient  [0..1]
uniform float specularStrength;  // specular coefficient [0..1]
uniform float shininess;         // Phong shininess exponent

// Material / texture
uniform int       useTexture;    // 1 = sample texture, 0 = use objectColor
uniform vec3      objectColor;   // used when useTexture == 0
uniform sampler2D texSampler;    // texture unit 0

out vec4 out_Color;

void main()
{
    // ---- Ambient ----
    vec3 ambient = ambientStrength * lightColor;

    // ---- Diffuse (Lambertian) ----
    vec3  norm     = normalize(fragNormal);
    vec3  lightDir = normalize(lightPos - fragPos);
    float diff     = max(dot(norm, lightDir), 0.0);
    vec3  diffuse  = diff * lightColor;

    // ---- Specular (Blinn-Phong half-vector) ----
    vec3  viewDir  = normalize(viewPos - fragPos);
    vec3  halfVec  = normalize(lightDir + viewDir);
    float spec     = pow(max(dot(norm, halfVec), 0.0), shininess);
    vec3  specular = specularStrength * spec * lightColor;

    // ---- Base colour ----
    vec3 baseColor;
    if (useTexture == 1) {
        vec4 texel = texture(texSampler, fragTexCoord);
        // Discard fully-transparent fragments (if texture has alpha)
        if (texel.a < 0.05) discard;
        baseColor = texel.rgb;
    } else {
        baseColor = objectColor;
    }

    // ---- Combine ----
    vec3 result = (ambient + diffuse + specular) * baseColor;
    out_Color   = vec4(result, 1.0);
}
