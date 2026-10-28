#version 450
// Yugioh Classic Remaster - cartas de campo (campos.h): la capa del tablero, UN cuadro sobre el tapete (sin buffers)
layout (set = 0, binding = 0) uniform UBO
{
	mat4 proyeccion;
	mat4 vista;
	mat4 luz;
	vec4 haciaSol;
	vec4 camara;
	vec4 pantalla;
} ubo;
layout (push_constant) uniform Empuje
{
	vec4 a;       // superficie, variante, capa A, capa B
	vec4 b;       // escala A, escala B, ambiente, mitad del ancho de la mesa
	vec4 tA;      // tinte A, mitad del fondo de la mesa
	vec4 tB;      // tinte B, mezcla de B
	vec4 color;   // color del campo
} empuje;

layout (location = 0) out vec3 outPos;

const vec2 ESQUINAS[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
                                vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

void main()
{
	vec2 q = ESQUINAS[gl_VertexIndex];
	// apenas sobre el tapete (su cara de arriba esta en y = 0)
	vec3 p = vec3(q.x * empuje.b.w, 0.006, q.y * empuje.tA.w);
	outPos = p;
	gl_Position = ubo.proyeccion * ubo.vista * vec4(p, 1.0);
}
