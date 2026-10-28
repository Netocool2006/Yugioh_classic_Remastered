#version 450
// Yugi Reload - carta de campo Bosque: particulas (ver bosque_particula.vert). Hojas y niebla van con alfa
// premultiplicado (tapan); luciernagas, polen y rayos de sol suman luz. Todo en el espacio de la pantalla.
layout (set = 0, binding = 0) uniform UBO
{
	mat4 proyeccion;
	mat4 vista;
	mat4 luz;
	vec4 haciaSol;
	vec4 camara;
	vec4 pantalla;
	vec4 bosque;
	vec4 bosque2;
} ubo;
layout (push_constant) uniform Empuje
{
	vec4 modo;       // x: tipo, y: intensidad (el ambiente del bosque)
} empuje;

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec3 inPosMundo;
layout (location = 2) flat in vec4 inColor;
layout (location = 3) flat in vec4 inExtra;

layout (location = 0) out vec4 outColor;

float azar(vec3 p)
{
	return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

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
	int tipo = int(empuje.modo.x + 0.5);
	float k = empuje.modo.y;
	vec2 q = inUV;
	float t = ubo.pantalla.z;
	if (tipo == 0) {
		// hoja: punta arriba y abajo, nervio central oscuro, borde suavizado
		float ancho = 0.62 * (1.0 - q.y * q.y) * (1.0 + 0.15 * q.y);
		float d = abs(q.x) - ancho;
		float aa = max(fwidth(d), 1e-3);
		float a = 1.0 - smoothstep(-aa, aa, d);
		if (a <= 0.0) {
			discard;
		}
		vec3 c = inColor.rgb * (0.8 + 0.25 * (1.0 - abs(q.x) / max(ancho, 1e-3)));
		c *= 1.0 - 0.35 * exp(-abs(q.x) * 40.0);
		a *= inColor.a * k;
		outColor = vec4(c * a, a);
	} else if (tipo == 1 || tipo == 2) {
		// resplandor redondo (luciernaga: nucleo y halo)
		float r2 = dot(q, q);
		float brillo = exp(-r2 * (tipo == 1 ? 6.0 : 4.0)) + (tipo == 1 ? exp(-r2 * 60.0) * 1.5 : 0.0);
		outColor = vec4(inColor.rgb * brillo * inColor.a * k * (tipo == 1 ? 1.0 : 0.45), 0.0);
	} else if (tipo == 3) {
		// niebla: borrosa, con ruido que se desplaza; se apaga hacia los bordes y abajo (contra el suelo)
		float borde = (1.0 - smoothstep(0.35, 1.0, length(q * vec2(1.0, 1.25))));
		borde *= smoothstep(-1.0, -0.55, q.y);
		vec3 p = vec3(q * vec2(inExtra.y * 0.35, inExtra.y * 0.15), inExtra.x * 17.0);
		float n = ruido(p + vec3(t * 0.08, 0.0, t * 0.03)) * 0.65 + ruido(p * 2.3 - vec3(t * 0.05, 0.0, 0.0)) * 0.35;
		float a = borde * smoothstep(0.25, 0.85, n) * inColor.a * k;
		outColor = vec4(inColor.rgb * a, a);
	} else {
		// rayo de sol: franja suave, mas fuerte en el medio de su largo, con vetas y polvo que brilla
		float lado = 1.0 - q.x * q.x;
		lado *= lado;
		float v = inExtra.y;
		float largo = smoothstep(0.0, 0.25, v) * (1.0 - smoothstep(0.55, 1.0, v));
		float vetas = 0.6 + 0.4 * ruido(vec3(q.x * 3.0, v * 2.0 - t * 0.15, inExtra.x * 9.0));
		float polvo = pow(ruido(inPosMundo * 6.0 + vec3(0.0, -t * 0.3, 0.0)), 8.0) * 3.0;
		outColor = vec4(inColor.rgb * lado * largo * (vetas + polvo) * 0.11 * inColor.a * k, 0.0);
	}
}
