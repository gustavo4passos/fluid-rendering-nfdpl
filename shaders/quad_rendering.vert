#version 450 core

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inTexCoord;

out vec2 f_TexCoord;

void main() 
{
    gl_Position = vec4(inPos, 0, 1);    
    f_TexCoord = inTexCoord;
}