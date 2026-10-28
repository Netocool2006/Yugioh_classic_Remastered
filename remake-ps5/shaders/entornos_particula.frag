#version 450
// Yugioh Classic Remaster - entornos por personaje rival: particulas (ver entornos_particula.vert). Alfa
// premultiplicado: hojas, nubes, niebla, humo y arena tapan; motas, chispas, datos y rafagas suman luz (alfa 0).
layout (set = 0, binding = 0) uniform UBO
{
	mat4 proyeccion;
	mat4 vista;
	mat4 luz;
	vec4 haciaSol;
	vec4 camara;
	vec4 pantalla;
} ubo;

layout (location = 0) in vec2 inUV;
layout (location = 1) flat in vec4 inColor;
layout (location = 2) flat in vec4 inExtra;
layout (location = 0) out vec4 outColor;

float azar(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float ruido(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(azar(i), azar(i + vec3(1, 0, 0)), f.x), mix(azar(i + vec3(0, 1, 0)), azar(i + vec3(1, 1, 0)), f.x), f.y),
	           mix(mix(azar(i + vec3(0, 0, 1)), azar(i + vec3(1, 0, 1)), f.x), mix(azar(i + vec3(0, 1, 1)), azar(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

void main()
{
	int tipo = int(inExtra.y + 0.5);
	vec2 q = inUV;
	float t = ubo.pantalla.z;
	float r2 = dot(q, q);
	if (tipo == 0 || tipo == 1) {
		float b = exp(-r2 * 5.0) + exp(-r2 * 40.0) * 1.2;
		outColor = vec4(inColor.rgb * b * inColor.a, 0.0);
	} else if (tipo == 2) {
		float ancho = 0.7 * (1.0 - q.y * q.y);
		float a = 1.0 - smoothstep(ancho - 0.08, ancho, abs(q.x));
		a *= inColor.a;
		if (a <= 0.0) discard;
		outColor = vec4(inColor.rgb * a, a);
	} else if (tipo == 3) {
		// la raya brilla mas en la punta de abajo (la cabeza que cae) y deja estela arriba
		float lado = 1.0 - q.x * q.x;
		float cabeza = smoothstep(1.0, -1.0, q.y);
		float bloques = step(0.35, fract(q.y * 6.0 + inExtra.x * 10.0));
		outColor = vec4(inColor.rgb * lado * (0.25 + cabeza * cabeza * 1.2) * bloques * inColor.a, 0.0);
	} else if (tipo == 5) {
		float b = (1.0 - q.y * q.y) * (1.0 - q.x * q.x);
		outColor = vec4(inColor.rgb * b * inColor.a, 0.0);
	} else if (tipo == 8) {
		float a = smoothstep(1.0, 0.3, sqrt(r2)) * inColor.a;
		outColor = vec4(inColor.rgb * a, a);
	} else {
		// nube, niebla y humo: borrosos con ruido que se desplaza
		float borde = 1.0 - smoothstep(0.3, 1.0, length(q * vec2(1.0, tipo == 6 ? 1.25 : 1.0)));
		if (tipo == 6) borde *= smoothstep(-1.0, -0.55, q.y);
		vec3 p = vec3(q * 1.6, inExtra.x * 17.0);
		float n = ruido(p + vec3(t * 0.08, t * 0.05, 0.0)) * 0.65 + ruido(p * 2.3 - vec3(t * 0.05, 0.0, 0.0)) * 0.35;
		float a = borde * smoothstep(0.2, 0.8, n) * inColor.a;
		vec3 c = inColor.rgb * (tipo == 4 ? (0.8 + 0.25 * q.y) : 1.0);
		outColor = vec4(c * a, a);
	}
}
