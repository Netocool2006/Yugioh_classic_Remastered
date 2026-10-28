#version 450
// Yugioh Classic Remaster - cartas de campo (campos.h): particulas cercanas sutiles a ras del tapete, sin buffers (cada
// instancia sale de un azar de su numero). Viven SOBRE el tablero pero entre las casillas: sobre una casilla se
// apagan, asi no tapan las cartas. Tipos:
//   0 motas de luz que flotan   1 brasas que suben   2 hojas o briznas que el viento arrastra   3 gotas y rocio que
//   saltan y brillan   4 polvo / arena a ras del suelo   5 bancos de niebla bajos   6 plumas que caen meciendose
//   7 chispas de plasma que chisporrotean   8 burbujas que suben
layout (set = 0, binding = 0) uniform UBO
{
	mat4 proyeccion;
	mat4 vista;
	mat4 luz;
	vec4 haciaSol;
	vec4 camara;
	vec4 pantalla;   // ancho, alto, tiempo, destello
} ubo;
layout (push_constant) uniform Empuje
{
	vec4 modo;       // x: tipo, y: cuantas, z: ambiente 0..1
	vec4 color;      // rgb, a: opacidad
	vec4 mesa;       // x, y: mitad del ancho y del fondo del tapete
} empuje;

layout (location = 0) out vec2 outUV;
layout (location = 1) flat out vec4 outColor;
layout (location = 2) flat out vec4 outExtra;   // x: semilla, y: tipo

const vec2 ESQUINAS[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
                                vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));
const float ESCALA = 1.4;
const vec2 MEDIA_ZONA = vec2(1.16, 1.62) * ESCALA * 0.5;

uint hash(uint x)
{
	x ^= x >> 16; x *= 0x7feb352du;
	x ^= x >> 15; x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}
float azar(uint i, uint k) { return float(hash(i * 13u + k * 0x9e3779b9u + 177u) & 0xffffffu) / 16777216.0; }

// 1 sobre una casilla (con margen), 0 lejos de todas
float sobreCasilla(vec2 p)
{
	const float COL[7] = float[](-8.2, -5.4, -2.7, 0.0, 2.7, 5.4, 8.2);
	const float FIL[4] = float[](-5.5, -2.2, 2.2, 5.5);
	float dx = 1e9, dz = 1e9;
	for (int i = 0; i < 7; i++) dx = min(dx, abs(p.x - COL[i] * ESCALA));
	for (int i = 0; i < 4; i++) dz = min(dz, abs(p.y - FIL[i] * ESCALA));
	vec2 q = vec2(dx, dz) - MEDIA_ZONA;
	return smoothstep(0.35, -0.05, max(q.x, q.y));
}

void main()
{
	vec2 q = ESQUINAS[gl_VertexIndex];
	uint i = uint(gl_InstanceIndex);
	int tipo = int(empuje.modo.x + 0.5);
	float t = ubo.pantalla.z;
	vec2 medio = empuje.mesa.xy - vec2(0.6);
	// ciclo de vida propio de cada una: nace en un lugar al azar del tapete y muere al terminar
	float dur = tipo == 5 ? 14.0 : (tipo == 1 || tipo == 8 ? 3.5 : 6.0);
	dur *= 0.7 + 0.6 * azar(i, 1u);
	float u = t / dur + azar(i, 2u);
	uint ciclo = uint(floor(u));
	float f = fract(u);
	uint s = i * 977u + ciclo * 131u;
	vec3 p = vec3((azar(s, 3u) * 2.0 - 1.0) * medio.x, 0.0, (azar(s, 4u) * 2.0 - 1.0) * medio.y);
	float tam = 0.05;
	float vida = sin(3.14159 * f);
	vec2 viento = vec2(0.6, 0.15);
	if (tipo == 0) {          // motas
		p += vec3(sin(t * 0.7 + float(i)) * 0.3, 0.15 + 0.9 * azar(s, 5u) + 0.25 * sin(t * 0.9 + float(i) * 1.7), cos(t * 0.6 + float(i)) * 0.3);
		tam = 0.035 + 0.03 * azar(s, 6u);
		vida *= 0.6 + 0.4 * sin(t * 3.0 + float(i) * 2.3);
	} else if (tipo == 1) {   // brasas
		p += vec3(sin(f * 6.0 + float(i)) * 0.2, f * (1.0 + 1.6 * azar(s, 5u)), cos(f * 5.0 + float(i)) * 0.2);
		tam = 0.025 + 0.025 * azar(s, 6u);
		vida *= 1.0 - f * 0.5;
	} else if (tipo == 2) {   // hojas / briznas
		// las rafagas del pasto de Sogen (campos.frag, rafagaSogen: la misma banda que viaja) las empujan y levantan
		float raf = smoothstep(0.3, 0.9, sin(dot(p.xz, normalize(viento)) * 0.32 - t * 1.25) * 0.5 + 0.5);
		p.xz += viento * f * 6.0 * (0.6 + 0.6 * azar(s, 5u)) * (0.7 + 0.6 * raf);
		p.y = 0.05 + 0.5 * abs(sin(f * 3.14159 * (1.0 + azar(s, 6u)))) * (0.8 + 0.5 * raf);
		tam = 0.07 + 0.04 * azar(s, 7u);
	} else if (tipo == 3) {   // gotas
		float salto = fract(f * 2.0);
		p.y = 0.02 + 0.45 * azar(s, 5u) * 4.0 * salto * (1.0 - salto);
		p.xz += vec2(azar(s, 6u) - 0.5, azar(s, 7u) - 0.5) * salto * 0.6;
		tam = 0.02 + 0.02 * azar(s, 8u);
		vida *= 0.5 + 0.5 * sin(t * 9.0 + float(i));
	} else if (tipo == 4) {   // polvo
		p.xz += viento * f * 9.0;
		p.y = 0.04 + 0.35 * azar(s, 5u) + 0.1 * sin(f * 9.0 + float(i));
		tam = 0.05 + 0.1 * azar(s, 6u);
	} else if (tipo == 5) {   // niebla
		p.xz += viento * f * 3.0;
		p.y = 0.15 + 0.2 * azar(s, 5u);
		tam = 1.4 + 1.2 * azar(s, 6u);
	} else if (tipo == 6) {   // plumas
		p.y = 2.0 * (1.0 - f) + 0.05;
		p.x += sin(f * 9.0 + float(i)) * 0.5;
		p.xz += viento * f * 2.0;
		tam = 0.09 + 0.04 * azar(s, 5u);
	} else if (tipo == 7) {   // chispas
		p.y = 0.05 + 0.5 * azar(s, 5u);
		p.xz += vec2(sin(t * 23.0 + float(i)), cos(t * 19.0 + float(i) * 3.0)) * 0.08;
		tam = 0.03 + 0.03 * azar(s, 6u);
		vida *= step(0.55, fract(sin(floor(t * 14.0) + float(i) * 7.3) * 43758.5));
	} else {                  // burbujas
		p.y = 0.03 + f * 0.8;
		p.x += sin(f * 12.0 + float(i)) * 0.06;
		tam = 0.03 + 0.03 * azar(s, 5u);
	}
	// fuera del tapete o sobre una casilla: se apaga (y la niebla, grande, solo se aclara)
	float fuera = step(abs(p.x), empuje.mesa.x - 0.2) * step(abs(p.z), empuje.mesa.y - 0.2);
	float casilla = sobreCasilla(p.xz);
	vida *= fuera * (1.0 - casilla * (tipo == 5 ? 0.6 : 1.0));
	vida *= empuje.modo.z;
	// cuadro de cara a la camara
	vec3 haciaCam = normalize(ubo.camara.xyz - p);
	vec3 der = normalize(cross(vec3(0.0, 1.0, 0.0), haciaCam));
	vec3 arr = cross(haciaCam, der);
	if (tipo == 2 || tipo == 6) {   // hojas y plumas giran
		float a = t * (1.5 + azar(s, 9u) * 2.0) + float(i);
		vec2 r = mat2(cos(a), -sin(a), sin(a), cos(a)) * q;
		q = r;
	}
	vec3 pos = p + (der * q.x + arr * q.y) * tam * (vida > 0.0 ? 1.0 : 0.0);
	outUV = ESQUINAS[gl_VertexIndex];
	outColor = vec4(empuje.color.rgb, empuje.color.a * max(vida, 0.0));
	outExtra = vec4(azar(s, 10u), float(tipo), 0.0, 0.0);
	gl_Position = ubo.proyeccion * ubo.vista * vec4(pos, 1.0);
}
