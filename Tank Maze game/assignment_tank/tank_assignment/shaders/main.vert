#version 330

// ============================================================
//  Vertex Shader — Blinn-Phong lighting pipeline
//  Attribute layout locations match hardcoded C++ constants:
//    ATTR_POS  = 0  (vec3 in_Position)
//    ATTR_NORM = 1  (vec3 in_Normal)
//    ATTR_TEX  = 2  (vec2 in_TexCoord)
// ============================================================

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_TexCoord;

// Matrices supplied per draw call
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projMatrix;
uniform mat3 normalMatrix;   // transpose(inverse(modelMatrix)) upper-3x3

// Outputs to fragment shader
out vec3 fragPos;       // world-space position
out vec3 fragNormal;    // world-space normal (normalised)
out vec2 fragTexCoord;

void main()
{
    vec4 worldPos  = modelMatrix * vec4(in_Position, 1.0);
    fragPos        = worldPos.xyz;
    fragNormal     = normalize(normalMatrix * in_Normal);
    fragTexCoord   = in_TexCoord;
    gl_Position    = projMatrix * viewMatrix * worldPos;
}
