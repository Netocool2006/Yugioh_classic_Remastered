#version 450
// Yugioh Classic Remaster - entornos por personaje rival (src/entornos.h): suelo, siluetas, ventanas, agua, publico,
// pantallas y llamas. Luz del entorno (sol, relleno del cielo y rebote del suelo) en espacio lineal, el mismo tono
// filmico y la misma vineta que la escena (escena.frag) y la niebla del entorno con la distancia (se pierde en el
// color del horizonte del fondo: entornos_cielo.frag).
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
	vec4 ent;        // x: peso visible, y: entorno, z: en su lugar
	vec4 entCieloA;  // rgb: cielo arriba (pantalla), a: estrellas
	vec4 entCieloH;  // rgb: horizonte (pantalla), a: nubes
	vec4 entNiebla;  // rgb: niebla (pantalla), a: densidad (1/m)
	vec4 entSolDir;  // xyz: sol o luna en el fondo, w: tamano
	vec4 entSolColor;// rgb: color del disco (pantalla), a: halo
	vec4 entLuz;     // rgb: luz del sol (lineal)
	vec4 entRelleno; // rgb: relleno del cielo (lineal), a: metros sin niebla
	vec4 entRebote;  // rgb: rebote del suelo (lineal), a: noche (ventanas encendidas 0..1)
	vec4 entEfecto;  // rgb: color del efecto propio (oricalco, datos...), a: intensidad
} ubo;
layout (push_constant) uniform Empuje
{
	vec4 modo;       // x: 0 opaco, 1 aditivo
} empuje;
layout (set = 0, binding = 1) uniform sampler2DShadow mapaSombra;
layout (set = 1, binding = 0) uniform sampler2DArray texturas;

layout (location = 0) in vec3 inPosMundo;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec4 inUV;
layout (location = 4) flat in float inMat;

layout (location = 0) out vec4 outColor;

const int SOLIDO = 0, LUZ = 1, VENTANAS = 2, HOLO = 3, SUELO = 4, AGUA = 5, PUBLICO = 6, FUEGO = 7, HALO = 8, PANTALLA = 9;
const int TIENDA = 1, CASTILLO = 2, ISLA = 3, PLAYA = 4, ACANTILADO = 5, LABERINTO = 6, RASCACIELOS = 7, CIUDAD = 8,
          DIRIGIBLE = 9, ALCATRAZ = 10, MUSEO = 11, NOAH = 12, ATLANTIDA = 13, ESTADIO = 14, TEMPLO = 15, SOMBRAS = 16;

float azar2(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float azar(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float ruido(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(azar(i), azar(i + vec3(1, 0, 0)), f.x), mix(azar(i + vec3(0, 1, 0)), azar(i + vec3(1, 1, 0)), f.x), f.y),
	           mix(mix(azar(i + vec3(0, 0, 1)), azar(i + vec3(1, 0, 1)), f.x), mix(azar(i + vec3(0, 1, 1)), azar(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float sombra(vec3 p)
{
	vec4 q = ubo.luz * vec4(p, 1.0);
	q.xyz /= q.w;
	vec2 uv = q.xy * 0.5 + 0.5;
	if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || q.z > 1.0) {
		return 1.0;
	}
	vec2 tx = 1.5 / vec2(textureSize(mapaSombra, 0));
	float s = texture(mapaSombra, vec3(uv + vec2(-tx.x, -tx.y), q.z - 0.002)) + texture(mapaSombra, vec3(uv + vec2(tx.x, -tx.y), q.z - 0.002)) +
	          texture(mapaSombra, vec3(uv + vec2(-tx.x, tx.y), q.z - 0.002)) + texture(mapaSombra, vec3(uv + vec2(tx.x, tx.y), q.z - 0.002));
	return s * 0.25;
}

// normal del mapa (capa + 1) con el marco tangente sacado de las derivadas (sin tangentes en la malla)
vec3 normalMapa(vec3 n, vec2 uv, float capa, out float rugosidad)
{
	vec4 m = texture(texturas, vec3(uv, capa + 1.0));
	rugosidad = m.a;
	vec3 dp1 = dFdx(inPosMundo), dp2 = dFdy(inPosMundo);
	vec2 du1 = dFdx(uv), du2 = dFdy(uv);
	vec3 dp2p = cross(dp2, n), dp1p = cross(n, dp1);
	vec3 T = dp2p * du1.x + dp1p * du2.x;
	vec3 B = dp2p * du1.y + dp1p * du2.y;
	float inv = inversesqrt(max(max(dot(T, T), dot(B, B)), 1e-12));
	vec3 t = m.xyz * 2.0 - 1.0;
	t.xy *= 0.9;
	return normalize(T * inv * t.x + B * inv * t.y + n * t.z);
}

// decoracion propia del suelo segun el entorno (emision, lineal)
vec3 decorarSuelo(vec3 p, inout vec3 albedo)
{
	int id = int(ubo.ent.y + 0.5);
	float t = ubo.pantalla.z;
	float r = length(p.xz);
	vec3 e = vec3(0.0);
	if (id == ATLANTIDA) {
		// el Sello del Oricalco: dos aros y un hexagrama verde alrededor de la mesa, que late
		float R = 21.0;
		float aros = exp(-abs(r - R) * 6.0) + exp(-abs(r - R - 1.6) * 7.0) * 0.8;
		float ang = atan(p.z, p.x);
		float estrella = 0.0;
		for (int k = 0; k < 6; k++) {
			float a = 1.0471976 * float(k) + 0.5235988;
			vec2 d = vec2(cos(a), sin(a));
			// cada lado del hexagrama: una recta a distancia R/2 del centro
			estrella += exp(-abs(dot(p.xz, d) - R * 0.5) * 8.0) * step(r, R);
		}
		float runas = step(0.55, ruido(vec3(ang * 40.0, r * 3.0, 1.0))) * smoothstep(0.6, 0.0, abs(r - R - 0.8));
		float late = 0.7 + 0.3 * sin(t * 1.4 - r * 0.15);
		e = ubo.entEfecto.rgb * (aros * 1.6 + estrella * 0.9 + runas * 0.7) * late * ubo.entEfecto.a;
		albedo *= 0.7;
	} else if (id == NOAH) {
		// reticula de datos: lineas verdes cada 2 m y pulsos que corren por ellas
		vec2 g = abs(fract(p.xz / 2.0) - 0.5);
		float linea = exp(-min(g.x, g.y) * 2.0 * 40.0);
		float pulso = pow(fract(p.x * 0.05 + t * 0.25 + floor(p.z / 2.0) * 0.37), 12.0) + pow(fract(p.z * 0.05 - t * 0.2 + floor(p.x / 2.0) * 0.21), 12.0);
		e = ubo.entEfecto.rgb * linea * (0.35 + 2.5 * pulso) * ubo.entEfecto.a;
		albedo = vec3(0.004, 0.012, 0.008);
	} else if (id == ESTADIO) {
		// arena: aros azules y la marca del centro
		float aros = exp(-abs(r - 19.0) * 5.0) + exp(-abs(r - 24.0) * 5.0) * 0.6;
		e = ubo.entEfecto.rgb * aros * (0.8 + 0.2 * sin(t * 2.0 + r)) * ubo.entEfecto.a;
	} else if (id == RASCACIELOS) {
		// helipuerto: aro amarillo pintado en la azotea
		float aro = smoothstep(0.35, 0.2, abs(r - 17.5));
		albedo = mix(albedo, vec3(0.55, 0.42, 0.05), aro * 0.8);
	} else if (id == DIRIGIBLE) {
		// cubierta: franjas de aviso al borde y luces que corren
		float borde = smoothstep(0.0, 0.3, abs(fract(r * 0.8) - 0.5)) * step(26.0, r);
		albedo = mix(albedo, vec3(0.6, 0.45, 0.05), step(26.0, r) * step(r, 28.0) * step(0.5, fract((p.x + p.z) * 0.5)) * 0.8);
		float luces = exp(-abs(r - 22.0) * 10.0) * pow(0.5 + 0.5 * sin(atan(p.z, p.x) * 60.0 - t * 6.0), 6.0);
		e = ubo.entEfecto.rgb * luces * 2.0 * ubo.entEfecto.a + borde * 0.0;
	} else if (id == SOMBRAS) {
		// grietas con brillo rojo apagado
		float g = 1.0 - abs(ruido(vec3(p.xz * 0.25, 3.0)) * 2.0 - 1.0);
		e = ubo.entEfecto.rgb * smoothstep(0.93, 0.99, g) * (0.6 + 0.4 * sin(t * 1.3 + p.x * 0.2)) * ubo.entEfecto.a;
	} else if (id == TEMPLO || id == MUSEO) {
		albedo *= 0.85 + 0.3 * ruido(vec3(p.xz * 0.15, 5.0));
	}
	return e;
}

vec3 colorAgua(vec3 p, vec3 v, out vec3 n)
{
	float t = ubo.pantalla.z;
	vec2 q = p.xz;
	float h1 = ruido(vec3(q * 0.35 + vec2(t * 0.25, t * 0.1), t * 0.2));
	float h2 = ruido(vec3(q * 1.1 - vec2(t * 0.4, -t * 0.3), t * 0.3));
	float hx = ruido(vec3((q + vec2(0.3, 0.0)) * 0.35 + vec2(t * 0.25, t * 0.1), t * 0.2)) - h1;
	float hz = ruido(vec3((q + vec2(0.0, 0.3)) * 0.35 + vec2(t * 0.25, t * 0.1), t * 0.2)) - h1;
	float hx2 = ruido(vec3((q + vec2(0.1, 0.0)) * 1.1 - vec2(t * 0.4, -t * 0.3), t * 0.3)) - h2;
	float hz2 = ruido(vec3((q + vec2(0.0, 0.1)) * 1.1 - vec2(t * 0.4, -t * 0.3), t * 0.3)) - h2;
	n = normalize(vec3(-(hx * 1.2 + hx2 * 0.9), 1.0, -(hz * 1.2 + hz2 * 0.9)));
	float fres = 0.04 + 0.96 * pow(1.0 - max(dot(n, v), 0.0), 5.0);
	vec3 r = reflect(-v, n);
	vec3 cielo = pow(mix(ubo.entCieloH.rgb, ubo.entCieloA.rgb, clamp(r.y * 1.5, 0.0, 1.0)), vec3(2.2));
	vec3 hondo = inColor.rgb * (0.4 + 0.6 * h1);
	vec3 brillo = pow(ubo.entSolColor.rgb, vec3(2.2)) * pow(max(dot(r, normalize(ubo.entSolDir.xyz)), 0.0), 300.0) * 6.0;
	// espuma donde toca la orilla o la plataforma (inColor.a: cuanta espuma)
	return mix(hondo, cielo, fres) + brillo;
}

void main()
{
	int mat = int(inMat + 0.5);
	float t = ubo.pantalla.z;
	vec3 v = normalize(ubo.camara.xyz - inPosMundo);
	vec3 n = normalize(inNormal);
	if (dot(n, v) < 0.0) {
		n = -n;
	}
	if (empuje.modo.x > 0.5) {
		// aditivo: llamas y halos (inUV.xy 0..1 en la tarjeta)
		vec2 q = inUV.xy * 2.0 - 1.0;
		vec3 c;
		if (mat == FUEGO) {
			float s = inUV.z * 7.0;
			float y = inUV.y;   // 0 abajo .. 1 arriba
			float n1 = ruido(vec3(q.x * 2.5, y * 3.0 - t * 3.2, s)) * 0.65 + ruido(vec3(q.x * 5.0, y * 6.0 - t * 5.0, s + 4.0)) * 0.35;
			float ancho = (1.0 - y) * 0.75 * (0.75 + 0.5 * n1);
			float forma = smoothstep(ancho, ancho * 0.25, abs(q.x)) * smoothstep(1.0, 0.25, y + (n1 - 0.5) * 0.6) * smoothstep(0.0, 0.12, y);
			vec3 base = mix(vec3(1.0, 0.25, 0.03), vec3(1.0, 0.85, 0.4), smoothstep(0.3, 1.0, forma));
			c = base * forma * inColor.rgb * inColor.a * (0.85 + 0.15 * sin(t * 13.0 + s));
		} else {
			float r2 = dot(q, q);
			c = inColor.rgb * inColor.a * exp(-r2 * 3.5) * (0.85 + 0.15 * sin(t * 2.0 + inUV.z * 9.0));
		}
		c *= ubo.ent.x;
		vec2 qq = gl_FragCoord.xy / ubo.pantalla.xy - 0.5;
		outColor = vec4(c * (1.0 - 0.35 * dot(qq, qq) * 2.0), 0.0);
		return;
	}

	vec3 albedo = inColor.rgb;
	float ao = inUV.w;
	float rug = 0.85;
	if (inUV.z >= 0.0) {
		vec4 tx = texture(texturas, inUV.xyz);
		albedo *= pow(tx.rgb, vec3(2.2));
		ao *= mix(1.0, tx.a, 0.8);
		if (mat != AGUA) {
			n = normalMapa(n, inUV.xy, inUV.z, rug);
		}
	}
	vec3 emision = vec3(0.0);
	float dist = length(ubo.camara.xyz - inPosMundo);
	if (mat == LUZ) {
		emision = inColor.rgb * inColor.a;
		albedo = vec3(0.0);
	} else if (mat == VENTANAS) {
		// ventanas: una cuadricula (u, v: una celda por unidad); unas encendidas al azar, de noche mas
		vec2 celda = floor(inUV.xy * vec2(1.0, 1.0) / 1.0);
		vec2 f = fract(inUV.xy);
		float hueco = step(0.18, f.x) * step(f.x, 0.82) * step(0.22, f.y) * step(f.y, 0.78);
		float encendida = step(1.0 - mix(0.18, 0.62, ubo.entRebote.a), azar2(celda + floor(inPosMundo.xz * 0.05) * 7.0));
		vec3 tibia = mix(vec3(1.0, 0.72, 0.38), vec3(0.65, 0.85, 1.0), step(0.7, azar2(celda * 1.7)));
		albedo = mix(albedo, vec3(0.03, 0.035, 0.05), hueco);
		emision = tibia * hueco * encendida * inColor.a * (0.9 + 0.1 * sin(t * 0.5 + celda.x));
	} else if (mat == HOLO) {
		// mundo de Noah: caras casi negras con aristas verdes que laten
		vec2 f = abs(fract(inUV.xy) - 0.5);
		float arista = exp(-(0.5 - max(f.x, f.y)) * 60.0);
		float barrido = pow(fract(inPosMundo.y * 0.08 - t * 0.3), 8.0);
		albedo = vec3(0.005, 0.015, 0.01);
		emision = inColor.rgb * inColor.a * (arista * 1.4 + barrido * 0.35 + 0.05);
	} else if (mat == SUELO) {
		emision = decorarSuelo(inPosMundo, albedo);
	} else if (mat == PUBLICO) {
		// gradas llenas: cabezas y ropa de colores (puntos por celda), con flashes de camaras
		vec2 celda = floor(inUV.xy * vec2(3.0, 2.0));
		vec3 ropa = vec3(azar2(celda), azar2(celda + 3.1), azar2(celda + 7.7));
		vec2 f = fract(inUV.xy * vec2(3.0, 2.0)) - 0.5;
		float cuerpo = smoothstep(0.42, 0.3, length(f * vec2(1.0, 0.8)));
		float salta = sin(t * (3.0 + azar2(celda) * 2.0) + azar2(celda + 1.0) * 6.28);
		albedo = mix(vec3(0.02), mix(ropa * 0.6, vec3(0.5, 0.35, 0.25), step(0.2, f.y + salta * 0.05)), cuerpo);
		float flash = step(0.9985, azar2(celda + floor(t * 6.0) * 0.37));
		emision = vec3(4.0) * flash * cuerpo;
	} else if (mat == PANTALLA) {
		// pantalla gigante: gradiente que corre con barras (sin texto ni logos)
		float u = inUV.x, w = inUV.y;
		vec3 c1 = inColor.rgb, c2 = inColor.rgb.brg;
		vec3 c = mix(c1, c2, 0.5 + 0.5 * sin(u * 6.0 + t * 1.5)) * (0.6 + 0.4 * step(0.5, fract(w * 12.0 - t * 2.0)));
		emision = c * inColor.a;
		albedo = vec3(0.0);
	}
	vec3 l = normalize(ubo.haciaSol.xyz);
	vec3 color;
	if (mat == AGUA) {
		vec3 nAgua;
		color = colorAgua(inPosMundo, v, nAgua);
	} else {
		float nl = max(dot(n, l), 0.0);
		float s = (mat == SUELO && dist < 45.0) ? sombra(inPosMundo) : 1.0;
		vec3 sol = ubo.entLuz.rgb * nl * s;
		vec3 relleno = mix(ubo.entRebote.rgb, ubo.entRelleno.rgb, n.y * 0.5 + 0.5) * ao;
		float brillo = pow(max(dot(n, normalize(l + v)), 0.0), mix(64.0, 8.0, rug)) * (1.0 - rug) * nl * s;
		color = albedo * (sol + relleno) + brillo * ubo.entLuz.rgb * 0.25 + emision;
	}
	color = vec3(1.0) - exp(-color * 1.15);
	vec2 q = gl_FragCoord.xy / ubo.pantalla.xy - 0.5;
	color *= 1.0 - 0.35 * dot(q, q) * 2.0;
	color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / 2.2));
	// niebla del entorno (en la pantalla, como el fondo): con la distancia y del todo antes del plano lejano (80 m)
	float f = 1.0 - exp(-max(dist - ubo.entRelleno.a, 0.0) * ubo.entNiebla.a);
	f = max(f, smoothstep(62.0, 78.0, dist));
	color = mix(color, ubo.entNiebla.rgb, f);
	outColor = vec4(color, 1.0);
}
