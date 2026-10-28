#version 450
// Yugi Reload - carta de campo Bosque: cesped. Sin buffers: cada instancia es una brizna (2 tramos y la punta,
// 15 vertices) que sale de un azar de su numero. Un 22 % crece en el borde del tablero (corto, sin tocar las
// casillas) y el resto en el suelo del bosque alrededor de la mesa (mas tupido cerca). Brota con la onda de la
// carta de campo, se mece con el viento y al retirarse se recoge hacia el suelo. Color: bosque.frag (modo 3).
layout (set = 0, binding = 0) uniform UBO
{
	mat4 proyeccion;
	mat4 vista;
	mat4 luz;
	vec4 haciaSol;
	vec4 camara;
	vec4 pantalla;   // ancho, alto, tiempo, destello
	vec4 bosque;     // x ambiente 0..1, y frente de crecimiento (m), z frente de retiro (m, < 0 nada), w onda
	vec4 bosque2;    // xy: origen de la onda (x, z), z: viento
} ubo;
layout (push_constant) uniform Empuje
{
	vec4 modo;       // x: 3 (cesped); z: mitad del ancho del tablero, w: mitad del fondo
} empuje;

layout (location = 0) out vec3 outPosMundo;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec4 outInfo;
layout (location = 4) out vec4 outUV;     // sin textura (capa < 0)

const float SUELO = -0.30;   // el suelo del bosque (la mesa esta encima, a 0)
const int PUNTO[15] = int[](0, 1, 2, 1, 3, 2, 2, 3, 4, 3, 5, 4, 4, 5, 6);
const float ALTURA[7] = float[](0.0, 0.0, 0.4, 0.4, 0.75, 0.75, 1.0);
const float LADO[7] = float[](-1.0, 1.0, -1.0, 1.0, -1.0, 1.0, 0.0);

uint hash(uint x)
{
	x ^= x >> 16; x *= 0x7feb352du;
	x ^= x >> 15; x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}
float azar(uint i, uint k) { return float(hash(i * 9u + k * 0x9e3779b9u) & 0xffffffu) / 16777216.0; }

// punto del perimetro de un rectangulo de medio ancho hx y medio fondo hz (t = 0..1 a lo largo)
vec2 perimetro(float hx, float hz, float t)
{
	float L = 4.0 * (hx + hz);
	float s = t * L;
	if (s < 2.0 * hx) return vec2(-hx + s, -hz);
	s -= 2.0 * hx;
	if (s < 2.0 * hz) return vec2(hx, -hz + s);
	s -= 2.0 * hz;
	if (s < 2.0 * hx) return vec2(hx - s, hz);
	s -= 2.0 * hx;
	return vec2(-hx, hz - s);
}

float rebote(float x)
{
	float c1 = 1.9, c3 = c1 + 1.0;
	float y = x - 1.0;
	return 1.0 + c3 * y * y * y + c1 * y * y;
}

void main()
{
	uint i = uint(gl_InstanceIndex);
	int k = PUNTO[gl_VertexIndex];
	float hx = empuje.modo.z, hz = empuje.modo.w;
	bool enMesa = azar(i, 0u) < 0.22;
	vec3 base;
	float alto, ancho;
	if (enMesa) {
		// borde del tablero: hacia adentro de 5 a 75 cm, lejos de las casillas
		float hacia = 0.05 + 0.7 * azar(i, 1u);
		vec2 q = perimetro(hx - hacia, hz - hacia, azar(i, 2u));
		base = vec3(q.x, 0.0, q.y);
		alto = 0.10 + 0.20 * azar(i, 3u) * (1.0 - hacia);
		ancho = 0.022 + 0.012 * azar(i, 4u);
	} else {
		// suelo del bosque: de la base de la mesa hacia afuera, mas tupido cerca
		float lejos = 0.3 + 30.0 * pow(azar(i, 1u), 1.7);
		vec2 q = perimetro(hx + 0.15 + lejos, hz + 0.15 + lejos, azar(i, 2u));
		q += (vec2(azar(i, 5u), azar(i, 6u)) - 0.5) * 1.2;
		base = vec3(q.x, SUELO, q.y);
		alto = 0.22 + 0.45 * azar(i, 3u) * (0.6 + 0.4 * azar(i, 7u));
		ancho = 0.03 + 0.025 * azar(i, 4u);
	}
	float d = length(base.xz - ubo.bosque2.xy);
	float jit = azar(i, 8u) * 4.0;
	float g = clamp((ubo.bosque.y - d - jit) / 9.0, 0.0, 1.0);
	float r = ubo.bosque.z < 0.0 ? 0.0 : clamp((ubo.bosque.z - d - jit) / 7.0, 0.0, 1.0);
	alto *= rebote(g) * (1.0 - r);
	float hf = ALTURA[k];
	float ang = azar(i, 9u) * 6.2832;
	vec3 lado = vec3(cos(ang), 0.0, sin(ang));
	vec3 inclina = vec3(-lado.z, 0.0, lado.x) * (0.25 + 0.35 * azar(i, 10u));
	float t = ubo.pantalla.z;
	vec2 viento = vec2(sin(t * 1.7 + base.x * 0.35 + base.z * 0.2) * 0.7 + sin(t * 3.1 + azar(i, 11u) * 6.28) * 0.3,
	                   sin(t * 1.3 + base.z * 0.3) * 0.3) * 0.18 * ubo.bosque2.z;
	vec3 p = base + lado * (LADO[k] * ancho * (1.0 - hf * 0.85)) + vec3(0.0, hf * alto, 0.0) +
	         (inclina * alto + vec3(viento.x, 0.0, viento.y) * alto * 2.0) * hf * hf;
	vec3 cara = normalize(cross(lado, inclina + vec3(0.0, 1.0, 0.0)));
	outPosMundo = p;
	outNormal = normalize(mix(vec3(0.0, 1.0, 0.0), cara, 0.45));
	// color: raiz oscura, punta clara; algunas amarillentas y otras azuladas
	// tonos naturales (pasto de sombra: oliva, algo seco, algo azulado) y que no compitan con el suelo escaneado
	float tono = azar(i, 12u);
	vec3 punta = tono < 0.18 ? vec3(0.50, 0.48, 0.24) : tono > 0.82 ? vec3(0.26, 0.40, 0.22) : vec3(0.34, 0.48, 0.17);
	punta *= 0.85 + 0.3 * azar(i, 13u);
	outColor = vec4(mix(vec3(0.07, 0.11, 0.03), punta, hf), 0.0);
	outUV = vec4(0.0, 0.0, -1.0, mix(0.55, 1.0, hf));
	outInfo = vec4(4.0, max(g, 0.001), 0.0, 0.25 + 0.75 * hf);
	gl_Position = ubo.proyeccion * ubo.vista * vec4(p, 1.0);
	if (alto < 0.004) {
		gl_Position = vec4(0.0, 0.0, -10.0, 1.0);
	}
}
