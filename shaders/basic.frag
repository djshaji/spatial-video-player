#version 330 core

in vec2 vUv;
out vec4 FragColor;

void main() {
    FragColor = vec4(vUv, 1.0 - vUv.x, 1.0);
}
