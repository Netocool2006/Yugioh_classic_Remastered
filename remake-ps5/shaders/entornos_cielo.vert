#version 450
// Yugi Reload - entornos: el fondo (un triangulo que cubre la pantalla, en la profundidad maxima:
// solo se ve donde la escena no dibujo nada). Pasa al fragmento la direccion de la mirada de cada pixel.
layout (push_constant) uniform Empuje
{
	mat4 inversa;    // (proyeccion * vista)^-1
	vec4 p;          // x: ambiente 0..1
} empuje;

layout (location = 0) out vec3 outDir;

void main()
{
	vec2 q = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;
	vec4 cerca = empuje.inversa * vec4(q, 0.0, 1.0);
	vec4 lejos = empuje.inversa * vec4(q, 1.0, 1.0);
	outDir = lejos.xyz / lejos.w - cerca.xyz / cerca.w;
	gl_Position = vec4(q, 1.0, 1.0);
}
