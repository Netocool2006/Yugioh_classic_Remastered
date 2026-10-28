#version 450
// Yugi Reload - carta de campo Bosque: particulas sin buffers (cada instancia sale de un azar de su numero):
//   0 hojas que caen girando (cuadrado orientado en 3D, forma de hoja en el fragmento)
//   1 luciernagas (resplandor que vaga y parpadea)
//   2 polen (motas finas que flotan)
//   3 niebla (bancos bajos y anchos de cara a la camara, de pie)
//   4 rayos de sol (franjas largas en la direccion del sol que cruzan el tablero)
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
	vec4 modo;       // x: tipo, y: intensidad, z: mitad del ancho del tablero, w: mitad del fondo
} empuje;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 outPosMundo;
layout (location = 2) flat out vec4 outColor;   // color y opacidad
layout (location = 3) flat out vec4 outExtra;   // por tipo (semilla, giro, ...)

const vec2 ESQUINAS[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
                                vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

uint hash(uint x)
{
	x ^= x >> 16; x *= 0x7feb352du;
	x ^= x >> 15; x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}
float azar(uint i, uint k) { return float(hash(i * 13u + k * 0x9e3779b9u + 77u) & 0xffffffu) / 16777216.0; }

// lo que ya alcanzo la onda de la carta (0..1): las particulas aparecen detras del frente
float alcanzado(vec3 p)
{
	float d = length(p.xz - ubo.bosque2.xy);
	return clamp((ubo.bosque.y - d) / 8.0, 0.0, 1.0);
}

void main()
{
	vec2 q = ESQUINAS[gl_VertexIndex];
	uint i = uint(gl_InstanceIndex);
	int tipo = int(empuje.modo.x + 0.5);
	float t = ubo.pantalla.z;
	float hx = empuje.modo.z, hz = empuje.modo.w;
	vec3 cam = ubo.camara.xyz;
	vec3 pos = vec3(0.0);
	vec3 ejeX = vec3(1.0, 0.0, 0.0), ejeY = vec3(0.0, 1.0, 0.0);
	vec4 color = vec4(1.0);
	vec4 extra = vec4(azar(i, 0u), 0.0, 0.0, 0.0);
	if (tipo == 0) {
		// hoja: cae de 11 m en un ciclo propio, se mece de lado y gira en dos ejes
		float periodo = 9.0 + 6.0 * azar(i, 1u);
		float u = fract(t / periodo + azar(i, 2u));
		vec3 inicio = vec3((azar(i, 3u) - 0.5) * 2.0 * (hx + 8.0), 11.0, (azar(i, 4u) - 0.7) * 2.0 * (hz + 6.0));
		bool sobreMesa = abs(inicio.x) < hx && abs(inicio.z) < hz;
		float suelo = sobreMesa ? 0.03 : -0.27;
		float cae = u * periodo * (1.0 + 0.4 * azar(i, 5u));
		pos = inicio + vec3(sin(t * 1.3 + azar(i, 6u) * 6.28) * 0.7 + cae * 0.15, -cae, cos(t * 0.9 + azar(i, 7u) * 6.28) * 0.4);
		float enSuelo = 0.0;
		if (pos.y < suelo) {
			enSuelo = clamp((suelo - pos.y) / 1.5, 0.0, 1.0);   // ya cayo: queda tendida y se desvanece
			pos.y = suelo;
		}
		float a1 = t * (1.2 + 2.0 * azar(i, 8u)) * (1.0 - enSuelo) + azar(i, 9u) * 6.28;
		float a2 = t * (0.8 + 1.5 * azar(i, 10u)) * (1.0 - enSuelo) + azar(i, 11u) * 6.28;
		vec3 u1 = vec3(cos(a1), sin(a1) * cos(a2), sin(a1) * sin(a2));
		vec3 u2 = normalize(cross(u1, vec3(sin(a2), cos(a2), 0.3)));
		if (enSuelo > 0.0) {
			u1 = normalize(vec3(u1.x, 0.0, u1.z + 1e-3));
			u2 = vec3(-u1.z, 0.0, u1.x);
		}
		float tam = 0.07 + 0.06 * azar(i, 12u);
		ejeX = u1 * tam;
		ejeY = u2 * tam * 1.6;
		float tono = azar(i, 13u);
		vec3 c = tono < 0.35 ? vec3(0.78, 0.36, 0.08) : tono < 0.6 ? vec3(0.86, 0.66, 0.16) : tono < 0.8 ? vec3(0.55, 0.20, 0.06) : vec3(0.42, 0.62, 0.15);
		// luz: de frente al sol mas clara
		float luz = 0.55 + 0.45 * abs(dot(normalize(cross(u1, u2)), normalize(ubo.haciaSol.xyz)));
		color = vec4(c * luz, (1.0 - enSuelo) * smoothstep(0.0, 0.04, u));
	} else if (tipo == 1) {
		// luciernaga: vaga cerca del suelo alrededor de la mesa y sobre el borde; parpadea
		float lejos = 14.0 * azar(i, 1u) * azar(i, 1u) - 0.6;
		vec2 b = vec2((azar(i, 2u) - 0.5) * 2.0, (azar(i, 3u) - 0.5) * 2.0);
		b = b / max(abs(b.x) / (hx + lejos), abs(b.y) / (hz + lejos));
		pos = vec3(b.x, 0.3 + 2.4 * azar(i, 4u), b.y) +
		      vec3(sin(t * 0.37 + azar(i, 5u) * 6.28) * 1.4, sin(t * 0.71 + azar(i, 6u) * 6.28) * 0.35, cos(t * 0.43 + azar(i, 7u) * 6.28) * 1.4);
		float brilla = pow(0.5 + 0.5 * sin(t * (1.2 + 1.6 * azar(i, 8u)) + azar(i, 9u) * 6.28), 5.0);
		vec3 aCam = normalize(cam - pos);
		ejeX = normalize(cross(vec3(0.0, 1.0, 0.0), aCam)) * 0.16;
		ejeY = normalize(cross(aCam, ejeX)) * 0.16;
		color = vec4(vec3(0.75, 1.0, 0.30) * (0.15 + 1.1 * brilla), 1.0);
	} else if (tipo == 2) {
		// polen: motas que flotan sobre el tablero y alrededor
		vec3 caja = vec3(hx + 4.0, 5.0, hz + 3.0);
		vec3 b = (vec3(azar(i, 1u), azar(i, 2u), azar(i, 3u)) - vec3(0.5, 0.0, 0.5)) * vec3(2.0, 1.0, 2.0) * caja;
		b += vec3(sin(t * 0.21 + azar(i, 4u) * 6.28), sin(t * 0.33 + azar(i, 5u) * 6.28) * 0.5, cos(t * 0.17 + azar(i, 6u) * 6.28)) * 1.2;
		b.y = mod(b.y + t * 0.12 * (0.5 + azar(i, 7u)), 5.0) + 0.2;
		pos = b;
		vec3 aCam = normalize(cam - pos);
		float tam = 0.025 + 0.025 * azar(i, 8u);
		ejeX = normalize(cross(vec3(0.0, 1.0, 0.0), aCam)) * tam;
		ejeY = normalize(cross(aCam, ejeX)) * tam;
		color = vec4(vec3(1.0, 0.95, 0.70) * (0.35 + 0.4 * azar(i, 9u)), 1.0);
	} else if (tipo == 3) {
		// banco de niebla: alrededor y sobre los bordes de la mesa, bajo, ancho, de pie mirando a la camara
		float ang = azar(i, 1u) * 6.2832;
		float lejos = 1.0 + 16.0 * azar(i, 2u);
		vec2 b = vec2(cos(ang), sin(ang));
		b = b / max(abs(b.x) / (hx + lejos), abs(b.y) / (hz + lejos));
		b += vec2(sin(t * 0.05 + azar(i, 3u) * 6.28), cos(t * 0.04 + azar(i, 4u) * 6.28)) * 2.5;
		float r = 4.0 + 5.0 * azar(i, 5u);
		pos = vec3(b.x, -0.3 + r * 0.35, b.y);
		vec3 aCam = cam - pos;
		aCam.y = 0.0;
		aCam = normalize(aCam + vec3(1e-4, 0.0, 0.0));
		ejeX = vec3(-aCam.z, 0.0, aCam.x) * r;
		ejeY = vec3(0.0, r * 0.38, 0.0);
		float cerca = smoothstep(5.0, 12.0, length(cam - pos));
		color = vec4(vec3(0.62, 0.74, 0.58), 0.22 * cerca);
		extra.y = r;
	} else {
		// rayo de sol: franja larga en la direccion del sol que cae sobre el tablero o cerca
		vec3 sol = normalize(ubo.haciaSol.xyz);
		vec3 abajo = vec3((azar(i, 1u) - 0.5) * 2.0 * (hx + 2.0), 0.0, (azar(i, 2u) - 0.55) * 2.0 * (hz + 1.0));
		float largo = 22.0;
		vec3 arriba = abajo + sol * largo;
		float v = q.y * 0.5 + 0.5;
		vec3 eje = arriba - abajo;
		vec3 centro = abajo + eje * v;
		vec3 aCam = normalize(cam - centro);
		vec3 lado = normalize(cross(normalize(eje), aCam));
		float ancho = 0.6 + 1.0 * azar(i, 3u);
		pos = centro + lado * q.x * ancho;
		// late despacio (las copas se mueven)
		float late = 0.55 + 0.45 * sin(t * (0.3 + 0.3 * azar(i, 4u)) + azar(i, 5u) * 6.28);
		color = vec4(vec3(1.0, 0.93, 0.62) * late, 1.0);
		extra.y = v;
		extra.z = largo;
		outUV = q;
		outPosMundo = pos;
		color.a *= alcanzado(abajo);
		outColor = color;
		outExtra = extra;
		gl_Position = ubo.proyeccion * ubo.vista * vec4(pos, 1.0);
		return;
	}
	color.a *= alcanzado(pos);
	vec3 p = pos + ejeX * q.x + ejeY * q.y;
	outUV = q;
	outPosMundo = p;
	outColor = color;
	outExtra = extra;
	gl_Position = ubo.proyeccion * ubo.vista * vec4(p, 1.0);
	if (color.a <= 0.002) {
		gl_Position = vec4(0.0, 0.0, -10.0, 1.0);
	}
}
