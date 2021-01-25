#version 330
// Input from vertex shader
in vec2 fragTexCoord;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 fragColor;

// Custom to replace shadertoy iTime
uniform float iTime;
uniform vec3 iResolution;

void main()
{
    fragColor = texture(texture0, fragTexCoord);
}
