#version 450 core

layout (location = 0) in vec3 particlePos;

uniform mat4 projection;
uniform mat4 view;

uniform float pointRadius;
// uniform float scale;

void main() 
{ 
                 
    // float dist = length(particlePos.xyz);
    // gl_PointSize = radius * (scale / dist);
    gl_PointSize = pointRadius;
    gl_Position =  projection * view * vec4(particlePos, 1.0);
}
