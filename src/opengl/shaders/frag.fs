#version 450 core

out vec4 FragColor;

in vec2 textureCoordinate;
uniform sampler2D tex;

void main()
{
    FragColor = texture(tex, textureCoordinate);
}