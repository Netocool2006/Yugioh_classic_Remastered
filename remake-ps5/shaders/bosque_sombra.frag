#version 450
// Yugi Reload - carta de campo Bosque: el mapa de sombras del bosque. Solo recorta el follaje por su alfa (las
// tarjetas de hojas dan la sombra de las hojas y no la de un rectangulo); lo demas escribe su profundidad.
layout (set = 1, binding = 0) uniform sampler2DArray texturas;

layout (location = 3) in vec4 inInfo;   // material, ...
layout (location = 4) in vec4 inUV;     // uv, capa de color

void main()
{
	if (inInfo.x > 0.5 && inInfo.x < 1.5 && inUV.z >= 0.0 && texture(texturas, inUV.xyz).a < 0.5) {
		discard;
	}
}
