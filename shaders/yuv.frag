#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uTexY;
uniform sampler2D uTexU;
uniform sampler2D uTexV;

void main() {
    float y = texture(uTexY, vUv).r;
    float u = texture(uTexU, vUv).r - 0.5;
    float v = texture(uTexV, vUv).r - 0.5;

    float r = y + 1.402 * v;
    float g = y - 0.344 * u - 0.714 * v;
    float b = y + 1.772 * u;

    FragColor = vec4(r, g, b, 1.0);
}
