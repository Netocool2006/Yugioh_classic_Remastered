#version 450
// Yugioh Classic Remaster - entornos por personaje rival: el fondo (un triangulo que cubre la pantalla, en la
// profundidad maxima: solo se ve donde nada se dibujo). Degradado del cielo, sol o luna con su halo, estrellas,
// nubes que pasan y el efecto propio de cada lugar (mar de nubes bajo el dirigible, reticula y datos del mundo de
// Noah, aurora verde del Oricalco, remolino del Reino de las Sombras, techo de los interiores). Abajo del horizonte,
// la niebla del entorno (el mismo color en el que se pierde su suelo). Sale en el espacio de la pantalla y se mezcla
// con alfa = peso del entorno (asi el cambio es un fundido).
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
	mat4 inversa;
	vec4 pano;       // xyz: de donde se ve el fondo (mundo); w: 0 sin imagen, 1 panorama pre-renderizado, 2 fondo plano
	vec4 panoInfo;   // x: ancho de la imagen (texeles); plano: y = ancho (rad), z = tan(abajo), w = tan(arriba)
	vec4 panoCielo;  // plano: rgb = color del cielo (pantalla) mas alla del borde de arriba de la imagen
} empuje;
layout (set = 1, binding = 1) uniform sampler2D panoColor;   // sRGB ya con su tono (pantalla)
layout (set = 1, binding = 2) uniform sampler2D panoProf;    // distancia desde el origen: log2(m) / 12; 1 = cielo

layout (location = 0) in vec3 inDir;
layout (location = 0) out vec4 outColor;

const float PI = 3.14159265;
bool plano() { return empuje.pano.w > 1.5; }
// panorama equirectangular (classic-escenarios)
vec2 uvPano(vec3 d) { return vec2(atan(d.x, -d.z) / (2.0 * PI) + 0.5, acos(clamp(d.y, -1.0, 1.0)) / PI); }
// fondo plano (classic-fondos): la imagen envuelta en un cilindro de radio 1 alrededor del origen: u por acimut (el
// centro mira hacia -z, detras del rival), v lineal en tan(elevacion) (las verticales quedan verticales, como un
// objetivo descentrado). Fuera de los costados se espeja (las camaras lateral y libre no ven un corte)
vec2 uvPlanoCrudo(vec3 d)
{
	float t = d.y / max(length(d.xz), 1e-4);
	return vec2(0.5 + atan(d.x, -d.z) / empuje.panoInfo.y, (empuje.panoInfo.w - t) / (empuje.panoInfo.w - empuje.panoInfo.z));
}
float espejo(float u) { return 1.0 - abs(1.0 - mod(u, 2.0)); }
vec2 uvFondo(vec3 d)
{
	if (!plano()) return uvPano(d);
	vec2 q = uvPlanoCrudo(d);
	return vec2(espejo(q.x), clamp(q.y, 0.0, 1.0));
}
float profPano(vec3 d)
{
	if (plano() && uvPlanoCrudo(d).y < 0.0) return -1.0;   // sobre la imagen: cielo
	float v = textureLod(panoProf, uvFondo(d), 0.0).r;
	return v > 0.99997 ? -1.0 : exp2(v * 12.0);
}

// Fondo pre-renderizado (rama classic-escenarios) o plano (classic-fondos): el rayo de la camara real se reproyecta a
// la imagen con su mapa de distancias (punto fijo: el punto del rayo que esta a la distancia guardada en su propia
// direccion). Desde el origen es exacto; al moverse la camara (CINE, libre) da el paralaje de cada cosa (en el plano,
// la profundidad estimada por Depth Anything: un paralaje sutil y creible). El cielo (sin distancia) queda en el
// infinito
vec3 fondoPano(vec3 v)
{
	vec3 o = ubo.camara.xyz - empuje.pano.xyz;
	vec3 dir = v;
	float b = dot(o, v), oo = dot(o, o);
	for (int i = 0; i < 5; i++) {
		float D = profPano(dir);
		if (D < 0.0) {
			dir = v;
			break;
		}
		float disc = b * b - oo + D * D;
		if (disc < 0.0) break;
		dir = normalize(o + v * (-b + sqrt(disc)));
	}
	float ang = max(length(dFdx(dir)), length(dFdy(dir)));
	if (!plano()) {
		float lod = clamp(log2(max(ang * empuje.panoInfo.x / (2.0 * PI), 1e-4)), 0.0, 14.0);
		return textureLod(panoColor, uvPano(dir), lod).rgb;
	}
	float lod = clamp(log2(max(ang * empuje.panoInfo.x / empuje.panoInfo.y, 1e-4)), 0.0, 14.0);
	vec2 q = uvPlanoCrudo(dir);
	vec3 c = textureLod(panoColor, vec2(espejo(q.x), clamp(q.y, 0.0, 1.0)), lod).rgb;
	// sobre el borde de arriba: se funde con el color del cielo; bajo el de abajo (tapado por la mesa): mas oscuro
	c = mix(c, empuje.panoCielo.rgb, smoothstep(0.0, -0.25, q.y) * 0.85);
	// (bajo el borde, la ultima fila estirada haria rayas: se funde con el suelo oscuro del color de la niebla)
	c = mix(c, ubo.entNiebla.rgb * 0.35, smoothstep(1.0, 1.12, q.y));
	// detras de la camara (la libre da la vuelta): la costura del espejo se esconde en el color del cielo
	c = mix(c, empuje.panoCielo.rgb * 0.8, smoothstep(2.6, 3.1, abs(atan(dir.x, -dir.z))) * 0.6);
	return c;
}

const int TIENDA = 1, CASTILLO = 2, ISLA = 3, PLAYA = 4, ACANTILADO = 5, LABERINTO = 6, RASCACIELOS = 7, CIUDAD = 8,
          DIRIGIBLE = 9, ALCATRAZ = 10, MUSEO = 11, NOAH = 12, ATLANTIDA = 13, ESTADIO = 14, TEMPLO = 15, SOMBRAS = 16;

float azar(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float ruido(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(azar(i), azar(i + vec3(1, 0, 0)), f.x), mix(azar(i + vec3(0, 1, 0)), azar(i + vec3(1, 1, 0)), f.x), f.y),
	           mix(mix(azar(i + vec3(0, 0, 1)), azar(i + vec3(1, 0, 1)), f.x), mix(azar(i + vec3(0, 1, 1)), azar(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
float fbm(vec3 p) { return ruido(p) * 0.5 + ruido(p * 2.03 + 3.7) * 0.27 + ruido(p * 4.1 + 9.1) * 0.15 + ruido(p * 8.3 + 1.3) * 0.08; }

void main()
{
	vec3 d = normalize(inDir);
	if (empuje.pano.w > 0.5) {
		vec3 f = fondoPano(d);
		vec2 qv = gl_FragCoord.xy / ubo.pantalla.xy - 0.5;
		outColor = vec4(f * (1.0 - 0.3 * dot(qv, qv) * 2.0), ubo.ent.x);
		return;
	}
	float t = ubo.pantalla.z;
	int id = int(ubo.ent.y + 0.5);
	float el = d.y;
	vec3 arriba = ubo.entCieloA.rgb, horiz = ubo.entCieloH.rgb, niebla = ubo.entNiebla.rgb;
	vec3 c = mix(horiz, arriba, pow(clamp(el, 0.0, 1.0), 0.55));
	// sol / luna y su halo
	vec3 sd = normalize(ubo.entSolDir.xyz);
	float cs = dot(d, sd);
	float disco = smoothstep(1.0 - ubo.entSolDir.w, 1.0 - ubo.entSolDir.w * 0.8, cs);
	float halo = pow(max(cs, 0.0), 24.0) * 0.5 + pow(max(cs, 0.0), 4.0) * 0.18;
	c += ubo.entSolColor.rgb * halo * ubo.entSolColor.a;
	// estrellas (por celdas de la direccion; titilan)
	if (ubo.entCieloA.a > 0.0 && el > 0.0) {
		vec3 g = d * 180.0;
		vec3 ci = floor(g);
		float a = azar(ci);
		vec3 centro = ci + 0.5 + (vec3(azar(ci + 1.3), azar(ci + 2.7), azar(ci + 5.1)) - 0.5) * 0.6;
		float e = smoothstep(0.35, 0.0, length(g - centro)) * step(0.985, a) * (0.6 + 0.4 * sin(t * (1.0 + a * 3.0) + a * 40.0));
		c += vec3(0.9, 0.95, 1.0) * e * ubo.entCieloA.a * smoothstep(0.0, 0.25, el);
	}
	// la luna no lleva estrellas encima y tapa: disco con manchas
	vec3 colDisco = ubo.entSolColor.rgb * (ubo.entCieloA.a > 0.0 ? (0.85 + 0.15 * ruido(d * 60.0)) : 1.6);
	c = mix(c, colDisco, disco);
	// nubes: un plano alto proyectado (se mueven con el viento); iluminadas del lado del sol
	if (ubo.entCieloH.a > 0.0 && el > 0.01) {
		vec2 p = d.xz / (el + 0.08) * 1.4 + vec2(t * 0.012, t * 0.004);
		float n = fbm(vec3(p, t * 0.01));
		float cub = smoothstep(1.0 - ubo.entCieloH.a * 0.75, 1.05 - ubo.entCieloH.a * 0.4, n);
		vec3 nube = mix(horiz * 0.75 + arriba * 0.1, mix(vec3(1.0), ubo.entSolColor.rgb, 0.35), 0.35 + 0.65 * pow(max(cs, 0.0), 2.0));
		nube = mix(nube * 0.82, nube, smoothstep(0.4, 0.9, fbm(vec3(p * 2.0 + 5.0, 1.0))));
		c = mix(c, nube, cub * smoothstep(0.01, 0.12, el) * (1.0 - disco));
	}
	// efectos propios
	if (id == NOAH) {
		// cupula digital: reticula en el cielo y columnas de datos que caen
		vec2 h = vec2(atan(d.x, -d.z) * 6.0, el * 18.0);
		vec2 g = abs(fract(h) - 0.5);
		float linea = exp(-(0.5 - max(g.x, g.y)) * 70.0) * smoothstep(0.0, 0.4, el);
		float col = floor(atan(d.x, -d.z) * 60.0);
		float cae = fract(el * 2.0 + t * (0.15 + azar(vec3(col)) * 0.3) + azar(vec3(col, 1.0, 0.0)));
		float datos = pow(cae, 14.0) * step(0.55, azar(vec3(col, 2.0, 0.0))) * step(0.5, ruido(vec3(col, floor(el * 120.0), 0.0)) + 0.3);
		c += ubo.entEfecto.rgb * (linea * 0.35 + datos * 0.9) * ubo.entEfecto.a;
	} else if (id == ATLANTIDA) {
		// aurora del Oricalco: cortinas verdes que ondulan
		float az = atan(d.x, -d.z);
		float cort = fbm(vec3(az * 2.5 + t * 0.03, t * 0.05, 1.0));
		float banda = smoothstep(0.08, 0.25, el) * smoothstep(0.75, 0.3, el) * smoothstep(0.45, 0.8, cort);
		float rayas = 0.6 + 0.4 * sin(az * 90.0 + cort * 20.0);
		c += ubo.entEfecto.rgb * banda * rayas * 0.55 * ubo.entEfecto.a;
	} else if (id == SOMBRAS) {
		// remolino oscuro sobre el templo, con vetas violetas y rojas
		vec3 cen = normalize(vec3(0.0, 0.55, -1.0));
		float r = acos(clamp(dot(d, cen), -1.0, 1.0));
		float a = atan(dot(d, vec3(1.0, 0.0, 0.0)), dot(d, cross(cen, vec3(1.0, 0.0, 0.0))));
		float espiral = fbm(vec3(a * 2.0 + r * 6.0 - t * 0.15, r * 3.0, 2.0));
		c = mix(c, c * 0.25, smoothstep(0.9, 0.1, r) * 0.7);
		c += ubo.entEfecto.rgb * smoothstep(0.55, 0.85, espiral) * smoothstep(1.2, 0.15, r) * 0.5 * ubo.entEfecto.a;
		c += vec3(0.6, 0.05, 0.02) * exp(-r * 9.0) * 0.6;
	} else if (id == DIRIGIBLE || id == ALCATRAZ) {
		// mar de nubes abajo (el dirigible) o el mar al atardecer muy abajo (Alcatraz)
		if (el < 0.0) {
			vec2 p = d.xz / (-el + 0.03) * 0.9 + vec2(t * 0.06, t * 0.02);
			float n = fbm(vec3(p, 0.0));
			vec3 mar;
			if (id == DIRIGIBLE) {
				mar = mix(niebla * 0.7, mix(vec3(0.95, 0.93, 1.0), ubo.entSolColor.rgb, 0.2), smoothstep(0.35, 0.75, n));
			} else {
				mar = mix(niebla * 0.55, niebla * 0.9, n) + ubo.entSolColor.rgb * pow(max(dot(reflect(d, vec3(0, 1, 0)), sd), 0.0), 40.0) * 0.6 * n;
			}
			c = mix(horiz, mar, smoothstep(0.0, -0.08, el));
		}
	}
	if (id == MUSEO || id == LABERINTO) {
		// interiores: sin cielo, un techo oscuro con un resplandor tibio de las lamparas
		c = mix(niebla, arriba, smoothstep(-0.05, 0.5, el)) + ubo.entSolColor.rgb * 0.15 * exp(-abs(el - 0.2) * 6.0);
	} else if (id != DIRIGIBLE && id != ALCATRAZ) {
		// abajo del horizonte: la niebla (se funde con el suelo lejano)
		c = mix(c, niebla, smoothstep(0.03, -0.04, el));
	}
	vec2 q = gl_FragCoord.xy / ubo.pantalla.xy - 0.5;
	c *= 1.0 - 0.3 * dot(q, q) * 2.0;
	outColor = vec4(c, ubo.ent.x);
}
