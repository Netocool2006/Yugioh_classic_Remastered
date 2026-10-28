#version 450
// Yugioh Classic Remaster - cartas de campo (campos.h): el material del tablero de cada carta de campo. Texturas PBR
// CC0 (arreglo: color + oclusion, normal GL + rugosidad) y lo propio de cada carta por codigo. Cubre el tapete con la
// misma onda del Bosque (ubo.bosque: frentes desde la carta) y deja ver las casillas (dentro se oscurece; el marco
// brilla con el color del campo). Luz, sombra, tono y niebla como escena.frag.
layout (set = 0, binding = 0) uniform UBO
{
	mat4 proyeccion;
	mat4 vista;
	mat4 luz;
	vec4 haciaSol;
	vec4 camara;
	vec4 pantalla;   // ancho, alto, tiempo, destello
	vec4 bosque;     // x ambiente del Bosque, y frente de crecimiento (m), z frente de retiro (m, < 0 nada), w onda
	vec4 bosque2;    // xy: origen de la onda (la carta de campo)
	vec4 ent;
	vec4 entCieloA;
	vec4 entCieloH;
	vec4 entNiebla;
	vec4 entSolDir;
	vec4 entSolColor;
	vec4 entLuz;
	vec4 entRelleno;
	vec4 entRebote;
	vec4 entEfecto;
} ubo;
layout (push_constant) uniform Empuje
{
	vec4 a;       // superficie, variante, capa A, capa B
	vec4 b;       // escala A, escala B, ambiente, mitad del ancho de la mesa
	vec4 tA;      // tinte A, mitad del fondo de la mesa
	vec4 tB;      // tinte B, mezcla de B
	vec4 color;   // color del campo (lineal); w > 1.5: tapete fijo (cobertura completa)
} empuje;
layout (set = 0, binding = 1) uniform sampler2DShadow mapaSombra;
layout (set = 1, binding = 0) uniform sampler2DArray texturas;

layout (location = 0) in vec3 inPos;
layout (location = 0) out vec4 outColor;

const int S_TEXTURA = 1, S_AGUA = 2, S_OSCURO = 3, S_NECRO = 4, S_MARMOL = 5, S_LAVA = 6, S_PLASMA = 7, S_ORICALCO = 8,
          S_METAL = 9, S_NUBES = 10;
const float ESCALA = 1.4;
const vec2 MEDIA_ZONA = vec2(1.16, 1.62) * ESCALA * 0.5;

float azar(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float azar2(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float ruido(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(azar(i), azar(i + vec3(1, 0, 0)), f.x), mix(azar(i + vec3(0, 1, 0)), azar(i + vec3(1, 1, 0)), f.x), f.y),
	           mix(mix(azar(i + vec3(0, 0, 1)), azar(i + vec3(1, 0, 1)), f.x), mix(azar(i + vec3(0, 1, 1)), azar(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
float fbm(vec3 p) { return ruido(p) * 0.55 + ruido(p * 2.03 + 7.1) * 0.28 + ruido(p * 4.1 + 3.3) * 0.17; }

// distancia al borde de la celda de Voronoi mas cercana (grietas de lava, de Pandemonium)
float bordeVoronoi(vec2 p)
{
	vec2 i = floor(p), f = fract(p);
	float d1 = 8.0, d2 = 8.0;
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 g = vec2(x, y);
			vec2 o = vec2(azar2(i + g), azar2(i + g + 17.3));
			float d = length(g + o - f);
			if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) { d2 = d; }
		}
	}
	return d2 - d1;
}

float sombra(vec3 posMundo, float nl)
{
	vec4 p = ubo.luz * vec4(posMundo, 1.0);
	p.xyz /= p.w;
	vec2 uv = p.xy * 0.5 + 0.5;
	if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || p.z > 1.0) return 1.0;
	float sesgo = mix(0.0015, 0.0004, nl);
	vec2 texel = 1.5 / vec2(textureSize(mapaSombra, 0));
	float s = texture(mapaSombra, vec3(uv + vec2(-0.5, -0.5) * texel, p.z - sesgo));
	s += texture(mapaSombra, vec3(uv + vec2(0.5, -0.5) * texel, p.z - sesgo));
	s += texture(mapaSombra, vec3(uv + vec2(-0.5, 0.5) * texel, p.z - sesgo));
	s += texture(mapaSombra, vec3(uv + vec2(0.5, 0.5) * texel, p.z - sesgo));
	return s * 0.25;
}

// la onda que sale de la carta de campo (la misma de escena.frag para el Bosque)
float cobertura(vec3 p, out float frente)
{
	float d = length(p.xz - ubo.bosque2.xy);
	float j = ruido(vec3(p.xz * 0.45, 3.1)) * 2.6 + ruido(vec3(p.xz * 1.7, 7.3)) * 0.8;
	float g = clamp((ubo.bosque.y - d - j) / 1.6, 0.0, 1.0);
	float r = ubo.bosque.z < 0.0 ? 0.0 : clamp((ubo.bosque.z - d - j) / 1.6, 0.0, 1.0);
	frente = exp(-abs(ubo.bosque.y - d - j) * 1.3) * (1.0 - r);
	if (ubo.bosque.z >= 0.0) frente += exp(-abs(ubo.bosque.z - d - j) * 1.3);
	frente *= ubo.bosque.w;
	return g * (1.0 - r);
}

// las casillas: la grilla de interfaz::ZONAS (7 columnas x 4 filas). dentro: 0..1; devuelve el brillo del marco
float casillas(vec2 p, out float dentro)
{
	const float COL[7] = float[](-8.2, -5.4, -2.7, 0.0, 2.7, 5.4, 8.2);
	const float FIL[4] = float[](-5.5, -2.2, 2.2, 5.5);
	float dx = 1e9, dz = 1e9;
	for (int i = 0; i < 7; i++) dx = min(dx, abs(p.x - COL[i] * ESCALA));
	for (int i = 0; i < 4; i++) dz = min(dz, abs(p.y - FIL[i] * ESCALA));
	vec2 q = vec2(dx, dz) - MEDIA_ZONA;
	float sd = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
	dentro = smoothstep(0.03, -0.05, sd);
	return 1.0 - smoothstep(0.012, 0.05, abs(sd));
}

struct Mat { vec3 albedo; vec3 n; float rugoso; float metal; vec3 emision; float ao; };

vec3 normalMapa(vec4 nm) { vec3 t = nm.xyz * 2.0 - 1.0; return normalize(vec3(t.x, t.z, t.y)); }   // GL (u -> x del mundo, v -> z)

Mat capa(vec2 p, float capaN, float escala, vec3 tinte)
{
	vec2 uv = p / escala;
	vec4 c = texture(texturas, vec3(uv, capaN));
	vec4 nm = texture(texturas, vec3(uv, capaN + 1.0));
	Mat m;
	m.albedo = pow(c.rgb, vec3(2.2)) * tinte;
	m.ao = c.a;
	m.n = normalMapa(nm);
	m.rugoso = nm.a;
	m.metal = 0.0;
	m.emision = vec3(0.0);
	return m;
}

Mat mezclar(Mat x, Mat y, float t)
{
	Mat m;
	m.albedo = mix(x.albedo, y.albedo, t);
	m.n = normalize(mix(x.n, y.n, t));
	m.rugoso = mix(x.rugoso, y.rugoso, t);
	m.metal = mix(x.metal, y.metal, t);
	m.emision = mix(x.emision, y.emision, t);
	m.ao = mix(x.ao, y.ao, t);
	return m;
}

// ---- Sogen: pasto con volumen ----
// El tablero es UN cuadro plano: las briznas salen de recorrer el rayo de la vista por una capa de ALTO_PASTO metros
// bajo la superficie (paralaje, PASOS_PASTO cortes) y preguntar en cada altura si hay una brizna (dos grillas de
// briznas conicas con azar por celda: altura, grosor, giro, color). Lo que no da con ninguna llega al suelo (oscuro).
// El viento dobla las briznas (desplazamiento ~ altura^2) con rafagas que recorren la mesa en la misma direccion que
// las hojas de campos_particula.vert. Sombra: oclusion por la profundidad + una prueba hacia el sol (2 consultas).
// De lejos (la brizna ocupa menos de ~1 pixel) se apaga el paralaje y queda el sombreado medio, sin parpadeo.
const float ALTO_PASTO = 0.11;
const int PASOS_PASTO = 10;
const vec2 VIENTO_DIR = vec2(0.970, 0.243);   // normalize(0.6, 0.15): el viento de las hojas

vec3 azar32(vec2 p)   // 3 azares por celda, sin seno (barato)
{
	vec3 q = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
	q += dot(q, q.yxz + 33.33);
	return fract((q.xxy + q.yzz) * q.zyx);
}

// rafagas: bandas que viajan con el viento, rotas en manchas por ruido (0 calma .. 1 rafaga)
float rafagaSogen(vec2 q, float t)
{
	float banda = sin(dot(q, VIENTO_DIR) * 0.32 - t * 1.25) * 0.5 + 0.5;
	float mancha = ruido(vec3(q * 0.18 - VIENTO_DIR * t * 0.45, t * 0.15));
	return smoothstep(0.35, 0.95, banda * 0.6 + mancha * 0.6);
}

// una grilla de briznas: > 0 si a la altura h (0 suelo .. 1 tope de la capa) en q hay brizna. id: azar de la brizna,
// lado: de que lado de la brizna (para la normal), alto: altura de la brizna (0..1)
float brizna(vec2 q, float h, float celda, vec2 corr, out vec3 id, out vec2 lado, out float alto)
{
	vec2 c = floor(q / celda + corr);
	vec2 f = fract(q / celda + corr);
	id = azar32(c);
	alto = 0.45 + 0.55 * id.z;
	vec2 d = f - (0.2 + 0.6 * id.xy);
	vec2 giro = normalize(vec2(id.x, fract(id.y * 7.31)) - 0.5 + 1e-4);   // giro al azar sin seno ni coseno
	vec2 r = vec2(giro.x * d.x - giro.y * d.y, giro.y * d.x + giro.x * d.y);
	r.y *= 0.42;   // hoja aplastada: mas ancha que gruesa
	float ancho = 0.24 * (1.0 - h / alto);
	lado = d;
	return h < alto ? ancho - length(r) : -1.0;
}

void pastoSogen(vec3 p, vec3 v, float t, inout Mat m)
{
	float raf = rafagaSogen(p.xz, t);
	// viento: empuje medio + rafaga + temblor fino
	vec2 viento = VIENTO_DIR * (0.25 + 0.75 * raf) + vec2(sin(t * 3.1 + p.x * 2.3), cos(t * 2.7 + p.z * 2.1)) * 0.07 * (0.4 + raf);
	vec3 base = m.albedo;
	// cuanto detalle cabe: la brizna (~1.5 cm) contra lo que mide un pixel en el tapete
	float pix = length(fwidth(p.xz));
	float detalle = 1.0 - smoothstep(0.012, 0.03, pix);
	vec3 rd = -v;
	vec2 avance = rd.xz / max(-rd.y, 0.2) * ALTO_PASTO;   // corrimiento al bajar toda la capa
	float hHit = 0.0, altoHit = 1.0;
	vec3 idHit = vec3(0.5);
	vec2 ladoHit = vec2(0.0), qHit = p.xz;
	bool dio = false;
	if (detalle > 0.0) {
		for (int i = 0; i < PASOS_PASTO; i++) {
			float h = 1.0 - (float(i) + 0.5) / float(PASOS_PASTO);
			vec2 q = p.xz + avance * (1.0 - h) - viento * ALTO_PASTO * 0.9 * h * h;
			vec3 id; vec2 lado; float alto;
			float b = brizna(q, h, 0.055, vec2(0.0), id, lado, alto);
			if (b <= 0.0) b = brizna(q * 1.17, h, 0.055, vec2(0.37, 0.61), id, lado, alto);
			if (b > 0.0) { hHit = h; altoHit = alto; idHit = id; ladoHit = lado; qHit = q; dio = true; break; }
		}
	}
	// color de la brizna: verde fresco / pasto seco por brizna, punta mas clara y amarilla, raiz oscura
	vec3 tinte = mix(vec3(0.80, 1.06, 0.55), vec3(1.18, 1.02, 0.55), smoothstep(0.62, 0.95, idHit.y)) * (0.8 + 0.35 * idHit.x);
	float punta = smoothstep(0.55, 1.0, hHit / altoHit);
	vec3 colBrizna = base * tinte * mix(vec3(1.0), vec3(1.35, 1.28, 0.8), punta * 0.6);
	float ao = mix(0.28, 1.0, pow(hHit, 0.75));
	// sombra de las briznas vecinas: una consulta hacia el sol, un poco mas arriba
	vec3 l = normalize(ubo.haciaSol.xyz);
	if (dio) {
		float hs = hHit + 0.3;
		if (hs < 1.0) {
			vec2 qs = qHit + l.xz / max(l.y, 0.25) * ALTO_PASTO * 0.3;
			vec3 i2; vec2 l2; float a2;
			if (brizna(qs, hs, 0.055, vec2(0.0), i2, l2, a2) > 0.0) ao *= 0.6;
		}
	}
	vec3 suelo = base * vec3(0.55, 0.45, 0.3) * 0.5;   // tierra y paja entre las raices
	vec3 albedoCerca = dio ? colBrizna : suelo;
	float aoCerca = dio ? ao : 0.3;
	// normal: hacia afuera de la brizna e inclinada con el viento (la rafaga peina el pasto y cambia el brillo)
	vec2 inclina = viento * (0.35 + 0.5 * hHit);
	vec3 nCerca = normalize(vec3(ladoHit.x * 1.4 + inclina.x, 0.8, ladoHit.y * 1.4 + inclina.y));
	// de lejos: el promedio (mas oscuro que una punta, con la rafaga aclarando)
	vec3 albedoLejos = base * vec3(0.95, 1.02, 0.8) * 0.85;
	m.albedo = mix(albedoLejos, albedoCerca, detalle);
	m.ao *= mix(0.72, aoCerca, detalle);
	m.n = normalize(mix(normalize(m.n + vec3(viento.x, 0.0, viento.y) * 0.25), nCerca, detalle));
	m.rugoso = mix(m.rugoso, 0.55, detalle);
	// la rafaga deja ver el reves claro de las hojas (las bandas que se ven correr)
	m.albedo *= 1.0 + 0.22 * raf;
	m.albedo = mix(m.albedo, m.albedo * vec3(1.2, 1.12, 0.75), smoothstep(0.6, 0.8, ruido(vec3(p.xz * 0.6, 2.0))) * 0.35);
}

vec3 cielo(vec3 r)
{
	// el cielo del entorno (colores de pantalla -> lineal) para los reflejos del agua, el oro y el marmol
	vec3 arriba = ubo.ent.x > 0.0 ? ubo.entCieloA.rgb : vec3(0.30, 0.45, 0.75);
	vec3 horiz = ubo.ent.x > 0.0 ? ubo.entNiebla.rgb : vec3(0.70, 0.75, 0.82);
	return pow(mix(horiz, arriba, clamp(r.y * 1.6, 0.0, 1.0)), vec3(2.2));
}

void main()
{
	vec3 p = inPos;
	float t = ubo.pantalla.z;
	int sup = int(empuje.a.x + 0.5);
	int var = int(empuje.a.y + 0.5);
	vec3 col = empuje.color.rgb;
	float frente;
	float cubre = cobertura(p, frente);
	if (empuje.color.w > 1.5) {   // rama classic-extras: tapete elegido (campos.h superficie fija): toda la mesa, sin onda
		cubre = 1.0;
		frente = 0.0;
	}
	// el canto del tapete queda a la vista
	float borde = smoothstep(0.0, 0.35, empuje.b.w - abs(p.x)) * smoothstep(0.0, 0.35, empuje.tA.w - abs(p.z));
	float alfa = cubre * borde * empuje.b.z;
	if (alfa <= 0.002 && frente * borde <= 0.002) discard;

	Mat m = capa(p.xz, empuje.a.z, empuje.b.x, empuje.tA.rgb);
	if (empuje.tB.w > 0.0) {
		float k = smoothstep(0.5 - 0.12, 0.5 + 0.12, fbm(vec3(p.xz * 0.22, 1.7)) + empuje.tB.w - 0.5);
		m = mezclar(m, capa(p.xz + 13.7, empuje.a.w, empuje.b.y, empuje.tB.rgb), k);
	}
	vec3 v = normalize(ubo.camara.xyz - p);
	float agua = 0.0;   // 0..1: cuanto es agua (reflejo, fresnel)

	if (sup == S_TEXTURA) {
		if (var == 1) {   // Poder de Gaia: venas de vida verdes que laten bajo el pasto
			float r = 1.0 - abs(ruido(vec3(p.xz * 0.35, 4.0)) * 2.0 - 1.0);
			float vena = smoothstep(0.93, 0.99, r) * (0.6 + 0.4 * sin(t * 1.8 - length(p.xz) * 0.6));
			m.emision += col * vena * 0.9;
		} else if (var == 2) {   // Coto de caza de las arpias: plumas sueltas sobre la roca
			vec2 c = floor(p.xz / 1.1);
			vec2 f = fract(p.xz / 1.1) - 0.5;
			float h = azar2(c);
			if (h > 0.55) {
				float a = h * 31.0;
				vec2 r = mat2(cos(a), -sin(a), sin(a), cos(a)) * (f - (vec2(azar2(c + 3.1), azar2(c + 5.7)) - 0.5) * 0.4);
				float pluma = smoothstep(1.0, 0.8, length(r / vec2(0.07, 0.24)));
				float raquis = smoothstep(0.012, 0.0, abs(r.x)) * step(abs(r.y), 0.24);
				m.albedo = mix(m.albedo, vec3(0.75, 0.62, 0.68) * (0.85 + 0.3 * ruido(vec3(r * 60.0, h))), pluma * 0.9);
				m.albedo = mix(m.albedo, vec3(0.35, 0.25, 0.25), raquis * 0.6);
			}
		} else if (var == 3) {   // Sogen: pradera con briznas de verdad (relieve por paralaje) y rafagas de viento
			pastoSogen(p, v, t, m);
		} else if (var == 4) {   // Montana: manchas de nieve
			// (roca gris: sin el verde del liquen) y nieve solo en manchas chicas, con borde sucio
			m.albedo = mix(m.albedo, vec3(dot(m.albedo, vec3(0.3, 0.59, 0.11))), 0.7);
			float nieve = smoothstep(0.66, 0.74, fbm(vec3(p.xz * 0.4, 5.0)) + (m.n.y - 0.9) * 0.3);
			m.albedo = mix(m.albedo, vec3(0.72, 0.75, 0.8) * (0.85 + 0.15 * ruido(vec3(p.xz * 6.0, 1.0))), nieve);
			m.rugoso = mix(m.rugoso, 0.45, nieve);
			m.n = normalize(mix(m.n, vec3(0, 1, 0), nieve * 0.7));
		} else if (var == 5) {   // Paramo: la tierra agrietada se reseca y el aire tiembla de calor
			m.albedo *= vec3(1.05, 0.95, 0.85);
			float polvo = fbm(vec3(p.xz * 0.12 + vec2(t * 0.15, 0.0), t * 0.05));
			m.albedo = mix(m.albedo, vec3(0.55, 0.42, 0.28), smoothstep(0.55, 0.8, polvo) * 0.35);
		} else if (var == 6) {   // Mundo jurasico: charcos que reflejan
			float charco = smoothstep(0.66, 0.7, fbm(vec3(p.xz * 0.15, 9.0)));
			m.albedo = mix(m.albedo, m.albedo * 0.35, charco);
			agua = charco * 0.7;
		}
	} else if (sup == S_AGUA) {
		// olas: suma de ondas direccionales (gradiente analitico) + rizos de ruido
		vec2 g = vec2(0.0);
		const vec2 DIR[5] = vec2[](vec2(0.8, 0.6), vec2(-0.5, 0.86), vec2(0.97, -0.25), vec2(-0.2, -0.98), vec2(0.6, 0.8));
		float altura = 0.0;
		float fuerza = var == 2 ? 1.5 : 1.0;
		for (int i = 0; i < 5; i++) {
			float k = 0.55 + 0.45 * float(i);
			float amp = 0.08 / (1.0 + float(i)) * fuerza;
			float fase = dot(DIR[i], p.xz) * k - t * (0.9 + 0.25 * float(i)) * sqrt(k);
			altura += amp * sin(fase);
			g += DIR[i] * k * amp * cos(fase);
		}
		float e = 0.06;
		vec3 q = vec3(p.xz * 2.2, t * 0.6);
		g += vec2(ruido(q + vec3(e, 0, 0)) - ruido(q - vec3(e, 0, 0)), ruido(q + vec3(0, e, 0)) - ruido(q - vec3(0, e, 0))) * 0.9;
		vec3 n = normalize(vec3(-g.x * 2.6, 1.0, -g.y * 2.6));   // (olas marcadas: el reflejo del cielo se rompe)
		vec3 profundo = empuje.tA.rgb;
		vec3 fondo = profundo;
		if (var == 1) {
			// Oceano legendario: agua clara y las ruinas (losas de piedra) debajo, con causticas
			vec2 refr = p.xz + n.xz * 0.6;
			Mat r = capa(refr, empuje.a.z, empuje.b.x, vec3(0.5, 0.95, 1.05));
			float c1 = ruido(vec3(refr * 1.6, t * 0.5)), c2 = ruido(vec3(refr * 1.9 + 4.0, -t * 0.45));
			float caustica = pow(1.0 - abs(c1 - c2), 8.0);
			fondo = mix(r.albedo * (0.45 + 1.6 * caustica) * vec3(0.45, 0.95, 1.0), profundo, 0.3);
		} else if (var == 2) {
			// Umiiruka: mar crecido turquesa, espuma en las crestas
			fondo = profundo * (1.0 + 0.5 * ruido(vec3(p.xz * 0.3, t * 0.2)));
		}
		// luz que atraviesa las crestas (verde azulado) y espuma fina rota en hilos solo en lo mas alto
		fondo += vec3(0.0, 0.05, 0.06) * smoothstep(0.0, 0.14, altura) * (var == 1 ? 0.5 : 1.0);
		float hilos = smoothstep(0.55, 0.85, ruido(vec3(p.xz * 4.5, t * 0.8)) * 0.6 + ruido(vec3(p.xz * 11.0, t)) * 0.4);
		float espuma = smoothstep(0.12, 0.17, altura) * hilos * (var == 2 ? 1.0 : 0.6);
		m.albedo = mix(fondo, vec3(0.75, 0.82, 0.85), clamp(espuma, 0.0, 1.0));
		m.n = n;
		m.rugoso = mix(0.06, 0.6, clamp(espuma, 0.0, 1.0));
		m.ao = 1.0;
		agua = 1.0 - clamp(espuma, 0.0, 1.0) * 0.8;
	} else if (sup == S_OSCURO) {
		// piedra oscura y niebla baja que se arrastra; Santuario oscuro: runas; Pandemonium: grietas rojas
		float niebla = fbm(vec3(p.xz * 0.16 + vec2(t * 0.12, t * 0.05), t * 0.08));
		m.albedo *= 0.75;
		if (var == 1) {
			float ang = atan(p.z, p.x), rad = length(p.xz);
			float aro = smoothstep(0.06, 0.0, abs(rad - 7.2)) + smoothstep(0.04, 0.0, abs(rad - 6.5));
			float runa = step(0.5, azar2(vec2(floor(ang * 12.0), 3.0))) * smoothstep(0.25, 0.0, abs(rad - 6.85)) *
			             step(0.35, fract(ang * 12.0 / 1.0)) * smoothstep(0.03, 0.0, abs(fract(ang * 24.0 + rad) - 0.5) - 0.2);
			m.emision += col * (aro + runa * 0.8) * (0.55 + 0.45 * sin(t * 1.3 - ang * 2.0));
		} else if (var == 2) {
			float b = bordeVoronoi(p.xz * 0.55);
			float grieta = exp(-b * 22.0) * (0.6 + 0.4 * ruido(vec3(p.xz * 0.8, t * 0.6)));
			m.albedo *= 1.0 - grieta * 0.7;
			m.emision += col * grieta * 1.4;
		}
		vec3 tono = var == 2 ? vec3(0.45, 0.06, 0.05) : col * 0.35;
		m.emision += tono * smoothstep(0.45, 0.85, niebla) * 0.35;
		m.albedo = mix(m.albedo, tono * 0.5, smoothstep(0.5, 0.9, niebla) * 0.5);
	} else if (sup == S_NECRO) {
		// losas con relieves (dibujos de trazos por celda) y arena que las cubre a manchas
		vec2 celda = floor(p.xz / 0.9);
		vec2 f = fract(p.xz / 0.9) - 0.5;
		float h = azar2(celda);
		float glifo = 0.0;
		if (h > 0.25) {
			glifo += smoothstep(0.03, 0.0, abs(length(f - vec2(0.0, 0.18)) - 0.12)) * step(0.5, fract(h * 7.0));            // ojo / sol
			glifo += smoothstep(0.025, 0.0, abs(f.x - (fract(h * 13.0) - 0.5) * 0.4)) * step(abs(f.y + 0.05), 0.3) * step(0.4, fract(h * 3.0));   // baston
			glifo += smoothstep(0.025, 0.0, abs(f.y + 0.28)) * step(abs(f.x), 0.32);                                          // linea de base
			glifo += smoothstep(0.03, 0.0, abs(length((f - vec2(0.12, -0.05)) * vec2(1.0, 1.8)) - 0.14)) * step(0.6, fract(h * 5.0));   // pajaro / ala
			glifo += smoothstep(0.035, 0.0, length(f - vec2(0.0, 0.18))) * step(0.5, fract(h * 7.0));                          // pupila
		}
		glifo = clamp(glifo, 0.0, 1.0) * step(abs(f.x), 0.42) * step(abs(f.y), 0.42);
		Mat arena = capa(p.xz + 7.0, empuje.a.w, empuje.b.y, empuje.tB.rgb);
		float cubreArena = smoothstep(0.42, 0.62, fbm(vec3(p.xz * 0.14 + vec2(t * 0.02, 0.0), 3.0)));
		m.albedo *= 1.0 - glifo * 0.75;
		m.n = normalize(m.n + vec3(0.0, 0.0, glifo * 0.4));
		m.emision += col * glifo * 0.12 * (0.6 + 0.4 * sin(t * 0.9 + h * 6.0));
		m = mezclar(m, arena, cubreArena);
	} else if (sup == S_MARMOL) {
		m.rugoso *= 0.5;
		if (var == 0) {   // Santuario en el cielo: nubes que pasan sobre el marmol y vetas de oro
			float nube = smoothstep(0.42, 0.8, fbm(vec3(p.xz * 0.12 + vec2(t * 0.25, t * 0.08), t * 0.05)));
			m.albedo = mix(m.albedo, vec3(1.0), nube * 0.85);
			m.emision += vec3(1.0, 0.95, 0.85) * nube * 0.18;
			agua = 0.25;
		} else if (var == 1) {   // Coro del santuario: incrustaciones de oro que brillan
			vec2 g = abs(fract(p.xz / 2.4 + 0.5) - 0.5) * 2.4;
			float linea = smoothstep(0.05, 0.0, min(g.x, g.y));
			m.albedo = mix(m.albedo, vec3(0.9, 0.62, 0.22), linea);
			m.metal = linea;
			m.emision += col * linea * (0.35 + 0.25 * sin(t * 1.5 + p.x * 0.4));
		} else if (var == 2) {   // Chispa luminosa: destellos que titilan
			vec2 c = floor(p.xz * 3.0);
			float h = azar2(c);
			float chispa = step(0.93, h) * smoothstep(0.18, 0.0, length(fract(p.xz * 3.0) - 0.5)) * pow(max(sin(t * (2.0 + h * 4.0) + h * 40.0), 0.0), 6.0);
			m.emision += vec3(1.3, 1.25, 1.0) * chispa * 3.0 + col * 0.06;
		} else {   // Formacion de luz reveladora: circulos de luz grabados en el suelo
			float rad = length(p.xz);
			float ang = atan(p.z, p.x);
			float aros = smoothstep(0.05, 0.0, abs(rad - 8.0)) + smoothstep(0.04, 0.0, abs(rad - 7.4)) + smoothstep(0.04, 0.0, abs(rad - 3.0));
			float rayos = smoothstep(0.035, 0.0, abs(fract(ang * 6.0 / 6.2832) - 0.5) * rad * 1.05 - 0.0) * step(3.0, rad) * step(rad, 7.4);
			vec2 sub = vec2(cos(ang), sin(ang));
			float chicos = 0.0;
			for (int i = 0; i < 6; i++) {
				float a = float(i) * 1.0472;
				chicos += smoothstep(0.04, 0.0, abs(length(p.xz - vec2(cos(a), sin(a)) * 5.2) - 0.9));
			}
			m.emision += col * (aros + rayos * 0.5 + chicos) * (0.7 + 0.3 * sin(t * 1.4 - rad * 0.5));
		}
	} else if (sup == S_LAVA) {
		// tierra quemada partida en placas; entre ellas la lava fluye y late
		vec2 q = p.xz * 0.42 + vec2(ruido(vec3(p.xz * 0.3, t * 0.1)) - 0.5) * 0.4;
		float b = bordeVoronoi(q);
		// grietas finas y de ancho variable (mas anchas donde el ruido abre rios de lava)
		float ancho = 0.025 + 0.09 * smoothstep(0.55, 0.85, ruido(vec3(p.xz * 0.18, 4.0)));
		float lava = 1.0 - smoothstep(ancho * 0.4, ancho, b);
		float flujo = ruido(vec3(p.xz * 1.2 + vec2(t * 0.35, t * 0.2), t * 0.3));
		vec3 caliente = mix(vec3(1.1, 0.14, 0.015), vec3(2.0, 0.75, 0.12), flujo * flujo);
		m.albedo = mix(m.albedo * 0.45, vec3(0.03, 0.012, 0.006), lava);
		m.emision += caliente * lava * (0.9 + 0.4 * sin(t * 2.0 + flujo * 6.0));
		m.emision += vec3(0.9, 0.18, 0.03) * exp(-b * 14.0) * 0.25;   // la roca se enrojece junto a la lava
		m.rugoso = mix(m.rugoso, 0.3, lava);
	} else if (sup == S_PLASMA) {
		// filamentos de plasma (crestas de ruido) que se mueven y destellan
		vec3 q = vec3(p.xz * 0.35, t * 0.25);
		float r1 = 1.0 - abs(ruido(q) * 2.0 - 1.0);
		float r2 = 1.0 - abs(ruido(q * 1.9 + 5.0) * 2.0 - 1.0);
		float fil = pow(r1, 60.0) + pow(r2, 80.0) * 0.6;   // (hilos finos, no gusanos)
		fil *= smoothstep(0.35, 0.65, ruido(vec3(p.xz * 0.12, t * 0.15)));   // a rafagas: tramos encendidos y tramos apagados
		float destello = 0.7 + 0.3 * sin(t * 13.0 + p.x) * sin(t * 7.0 + p.z);
		m.albedo *= 0.6;
		m.emision += mix(col, vec3(1.2, 0.8, 1.6), 0.3) * fil * 2.0 * destello;
		m.emision += col * 0.05 * fbm(vec3(p.xz * 0.2, t * 0.2));
		m.metal = 0.3;
	} else if (sup == S_ORICALCO) {
		// el sello: un circulo doble con el hexagrama, centrado en la mesa, que late en verde
		vec2 q = p.xz;
		float R = 9.2;
		float rad = length(q);
		float lineas = smoothstep(0.09, 0.0, abs(rad - R)) + smoothstep(0.06, 0.0, abs(rad - (R - 0.55))) + smoothstep(0.05, 0.0, abs(rad - 2.0));
		// hexagrama: dos triangulos inscritos en R - 0.55 (6 rectas: distancia del centro = (R - 0.55) / 2)
		float hex = 0.0;
		for (int i = 0; i < 6; i++) {
			float a = float(i) * 1.0472 + 0.5236;
			vec2 nrm = vec2(cos(a), sin(a));
			vec2 tan_ = vec2(-nrm.y, nrm.x);   // (cada lado del triangulo: solo entre sus dos vertices)
			hex += smoothstep(0.07, 0.0, abs(dot(q, nrm) - (R - 0.55) * 0.5)) * step(abs(dot(q, tan_)), (R - 0.55) * 0.866);
		}
		float ang = atan(q.y, q.x);
		float glifos = step(0.45, azar2(vec2(floor(ang * 40.0 / 6.2832 * 3.0), 1.0))) * step(R - 0.5, rad) * step(rad, R - 0.05) *
		               smoothstep(0.04, 0.0, abs(fract(ang * 40.0 / 6.2832 * 3.0) - 0.5) - 0.3);
		float trazo = clamp(lineas + hex + glifos * 0.7, 0.0, 1.5);
		float late = 0.75 + 0.25 * sin(t * 2.2 - rad * 0.35);
		m.albedo *= 0.7;
		m.emision += col * trazo * 1.6 * late;
		m.emision += col * 0.06 * smoothstep(R + 1.0, 0.0, rad) * late;   // resplandor dentro del sello
	} else if (sup == S_METAL) {
		m.metal = 0.85;
		m.rugoso *= 0.6;
		float rad = length(p.xz);
		float ang = atan(p.z, p.x);
		if (var == 0) {   // Puerta de fusion: un vortice en espiral
			float espiral = sin(ang * 3.0 + log(rad + 0.3) * 6.0 - t * 2.2);
			float brazo = smoothstep(0.75, 1.0, espiral) * smoothstep(9.5, 1.0, rad);
			vec3 c = mix(col, vec3(1.3, 0.9, 0.4), 0.5 + 0.5 * sin(ang * 2.0 + t));
			m.emision += c * brazo * 1.1 + col * smoothstep(2.0, 0.0, rad) * 1.2;
		} else if (var == 1) {   // Campo centrifugo: anillos concentricos con segmentos que giran
			float banda = fract(rad / 1.2);
			float aro = smoothstep(0.05, 0.0, abs(banda - 0.5) - 0.02);
			float giro = ang + t * (mod(floor(rad / 1.2), 2.0) < 1.0 ? 0.8 : -0.6) / max(rad, 1.0) * 4.0;
			float segmento = step(0.5, fract(giro * 6.0 / 6.2832));
			m.emision += col * aro * (0.25 + 0.75 * segmento) * step(rad, 9.6);
			m.albedo *= 0.85 + 0.15 * step(0.5, banda);
		} else {   // Castillo dorado: oro pulido en baldosas
			m.albedo = mix(m.albedo, vec3(1.0, 0.71, 0.29), 0.6);
			m.metal = 1.0;
			m.rugoso *= 0.5;
			m.emision += col * 0.03;
		}
	} else if (sup == S_NUBES) {
		// Corriente de aire ascendente: un mar de nubes que corre con el viento
		vec3 q = vec3(p.xz * 0.11 + vec2(t * 0.35, t * 0.12), t * 0.07);
		float d = fbm(q);
		float d2 = fbm(q * 2.1 + 3.0);
		float alto = smoothstep(0.35, 0.8, d * 0.7 + d2 * 0.3);
		m.albedo = mix(vec3(0.42, 0.55, 0.75), vec3(0.95, 0.97, 1.0), alto);
		float e = 0.25;
		float dx = fbm(q + vec3(e * 0.11, 0, 0)) - d, dz = fbm(q + vec3(0, e * 0.11, 0)) - d;
		m.n = normalize(vec3(-dx * 18.0, 1.0, -dz * 18.0));
		m.rugoso = 0.9;
		m.ao = 0.75 + 0.25 * alto;
		float raya = smoothstep(0.96, 1.0, sin(p.x * 0.8 + p.z * 0.2 - t * 6.0 + ruido(vec3(p.xz * 0.4, 0.0)) * 8.0));
		m.emision += vec3(1.0) * raya * 0.08;
		m.emision += vec3(1.0, 0.98, 0.9) * alto * 0.12;
	}

	// las casillas: dentro, el material mas oscuro (las cartas se leen); el marco brilla con el color del campo
	float dentro;
	float marco = casillas(p.xz, dentro);

	// luz (como escena.frag, con la del entorno de la carta)
	vec3 n = normalize(m.n);
	vec3 l = normalize(ubo.haciaSol.xyz);
	float nl = max(dot(n, l), 0.0);
	float s = sombra(p, nl);
	vec3 sol = vec3(1.0, 0.93, 0.82) * 2.4 * nl * s;
	vec3 relleno = mix(vec3(0.10, 0.08, 0.07), vec3(0.32, 0.38, 0.52), n.y * 0.5 + 0.5);
	if (ubo.ent.x > 0.0) {
		sol = mix(sol, ubo.entLuz.rgb * nl * s, ubo.ent.x);
		relleno = mix(relleno, mix(ubo.entRebote.rgb, ubo.entRelleno.rgb, n.y * 0.5 + 0.5), ubo.ent.x);
	}
	vec3 h = normalize(l + v);
	float brillo = pow(max(dot(n, h), 0.0), mix(160.0, 12.0, m.rugoso)) * (1.0 - m.rugoso * 0.85) * nl * s;
	vec3 spec = mix(vec3(0.04), m.albedo, m.metal);
	vec3 difuso = m.albedo * (1.0 - m.metal * 0.9);
	vec3 color = difuso * (sol + relleno) * m.ao + brillo * spec * 6.0 * (sol.r + sol.g + sol.b + 0.5);
	// reflejo del cielo: agua, metal y marmol pulido (fresnel)
	float fres = pow(1.0 - max(dot(n, v), 0.0), 5.0);
	vec3 refl = cielo(reflect(-v, n)) * mix(1.0, 0.75, agua);
	float cuantoRefl = max(agua * (0.02 + 0.45 * fres), m.metal * mix(0.5, 1.0, fres) * (1.0 - m.rugoso));
	color = mix(color, refl * mix(vec3(1.0), m.albedo * 1.5 + 0.2, m.metal), clamp(cuantoRefl, 0.0, 1.0));
	if (agua > 0.0) {
		color += vec3(1.0, 0.95, 0.85) * pow(max(dot(n, h), 0.0), 400.0) * 18.0 * nl * s * agua;   // destellos del sol
	}
	color += m.emision;
	// casillas: oscuras por dentro, marco del color del campo
	color *= mix(1.0, 0.5, dentro);
	color += col * marco * (0.35 + 0.15 * sin(t * 1.6 + p.x * 0.3));
	// el frente de la onda
	color += col * frente * 1.6;

	// tono filmico suave y vineta (como escena.frag)
	color = vec3(1.0) - exp(-color * 1.15);
	vec2 qv = gl_FragCoord.xy / ubo.pantalla.xy - 0.5;
	color *= 1.0 - 0.35 * dot(qv, qv) * 2.0;
	outColor.rgb = pow(clamp(color, 0.0, 1.0), vec3(1.0 / 2.2));
	if (ubo.ent.x > 0.0) {
		float d = length(ubo.camara.xyz - p);
		float f = (1.0 - exp(-max(d - ubo.entRelleno.a, 0.0) * ubo.entNiebla.a)) * ubo.ent.x;
		outColor.rgb = mix(outColor.rgb, ubo.entNiebla.rgb, f);
	}
	outColor.rgb += ubo.pantalla.w * vec3(1.0, 0.85, 1.0);   // destello del impacto
	// el frente de la onda se ve aunque todavia no cubra: delante del material, solo su brillo
	outColor.a = alfa;
	float fa = clamp(frente * borde * 0.8, 0.0, 1.0);
	if (fa > alfa) {
		vec3 brilloFrente = pow(vec3(1.0) - exp(-col * 2.0), vec3(1.0 / 2.2));
		outColor.rgb = mix(brilloFrente, outColor.rgb, alfa / fa);
		outColor.a = fa;
	}
}
