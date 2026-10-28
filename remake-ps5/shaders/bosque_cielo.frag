#version 450
// Yugi Reload - carta de campo Bosque: el fondo de bosque profundo, por pixel segun hacia donde mira la camara.
// Cuatro capas de copas y troncos (de lejos a cerca, cada una menos tapada por la niebla), luz dorada que se filtra
// por arriba con huecos entre las hojas, rayos de sol que bajan entre las copas y niebla verdosa abajo (el mismo
// color en el que se pierde el suelo del bosque). Sale ya en el espacio de la pantalla (con gamma); la interfaz
// pinta encima el fondo de siempre con alfa 1 - ambiente (asi el cambio es un fundido).
layout (set = 0, binding = 0) uniform UBO
{
	mat4 proyeccion;
	mat4 vista;
	mat4 luz;
	vec4 haciaSol;
	vec4 camara;
	vec4 pantalla;   // ancho, alto, tiempo, destello
	vec4 bosque;
	vec4 bosque2;
} ubo;

layout (location = 0) in vec3 inDir;
layout (location = 0) out vec4 outColor;

const vec3 NIEBLA_BOSQUE = vec3(0.40, 0.50, 0.38);
const float PI = 3.14159265;

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

float fbm(vec3 p)
{
	return ruido(p) * 0.55 + ruido(p * 2.1 + 3.7) * 0.28 + ruido(p * 4.3 + 9.1) * 0.17;
}

void main()
{
	vec3 dir = normalize(inDir);
	float t = ubo.pantalla.z;
	float el = dir.y;                                  // seno de la elevacion
	vec2 h = normalize(dir.xz + vec2(1e-5));
	float az = atan(h.x, -h.y);                         // 0 = hacia el fondo (-z)
	// cielo filtrado por las copas: claro y dorado cerca del horizonte, verde oscuro arriba con huecos de luz
	vec3 c = mix(vec3(0.78, 0.82, 0.58), vec3(0.20, 0.33, 0.18), smoothstep(0.02, 0.45, el));
	float hueco = smoothstep(0.62, 0.78, fbm(vec3(h * 6.0, el * 9.0)));
	c = mix(c, vec3(0.08, 0.16, 0.08), smoothstep(0.25, 0.6, el) * (1.0 - hueco * 0.8));
	// rayos de sol entre las copas: abanico que baja desde un claro alto, detras del tablero
	vec2 foco = vec2(0.25, 0.55);
	vec2 rel = vec2(az, el) - foco;
	float ang = atan(rel.x, -rel.y);
	float rayos = pow(ruido(vec3(ang * 9.0, t * 0.05, 1.0)), 3.0) * 0.8 + pow(ruido(vec3(ang * 23.0, t * 0.08, 4.0)), 4.0) * 0.5;
	rayos *= smoothstep(1.4, 0.2, length(rel)) * smoothstep(-0.05, 0.12, el);
	// capas de arboles: de lejos (0) a cerca (3)
	float tapa = 0.0;
	for (int k = 0; k < 4; k++) {
		float fk = float(k);
		float R = 2.2 + 2.6 * fk;
		float copa = 0.03 + 0.045 * fk + 0.13 * fbm(vec3(h * R, fk * 7.3));
		// puntas de pinos (periodico en el angulo)
		float n = 18.0 + 10.0 * fk;
		float punta = max(0.0, 1.0 - abs(fract(az / (2.0 * PI) * n + ruido(vec3(h * 3.0, fk)) ) - 0.5) * 4.0);
		copa += punta * (0.02 + 0.012 * fk) * step(0.5, ruido(vec3(h * 5.0, fk + 2.0)));
		if (el < copa) {
			vec3 capa = mix(NIEBLA_BOSQUE, vec3(0.05, 0.12, 0.07), 0.22 + 0.2 * fk);
			capa *= 0.85 + 0.3 * ruido(vec3(h * 40.0, el * 30.0 + fk));
			// troncos en las capas cercanas, bajo las copas
			if (k >= 1) {
				float m = 28.0 + 16.0 * fk;
				float x = fract(az / (2.0 * PI) * m + 0.5 * ruido(vec3(h * 2.0, fk + 5.0)));
				float ancho = 0.05 + 0.04 * ruido(vec3(floor(az / (2.0 * PI) * m), fk, 2.0));
				float tronco = smoothstep(ancho, ancho * 0.5, abs(x - 0.5)) * step(el, copa - 0.05 - 0.02 * fk);
				capa = mix(capa, mix(NIEBLA_BOSQUE, vec3(0.06, 0.05, 0.03), 0.35 + 0.18 * fk), tronco * smoothstep(-0.2, 0.0, el));
			}
			c = capa;
			tapa = fk / 3.0;
		}
	}
	c += vec3(1.0, 0.92, 0.6) * rayos * (1.0 - tapa * 0.7) * 0.35;
	// abajo: el suelo perdido en la niebla
	c = mix(c, NIEBLA_BOSQUE * 0.92, smoothstep(0.02, -0.08, el));
	// la misma vineta de la escena
	vec2 q = gl_FragCoord.xy / ubo.pantalla.xy - 0.5;
	c *= 1.0 - 0.3 * dot(q, q) * 2.0;
	outColor = vec4(c, 1.0);
}
