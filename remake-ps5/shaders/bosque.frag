#version 450
// Yugi Reload - carta de campo Bosque: el color de todo lo solido del bosque (bosque.vert y bosque_cesped.vert).
// Realista (rama bosque-real): texturas PBR escaneadas (Poly Haven / ambientCG, CC0) en un arreglo 2D (set 1):
// corteza, follaje con alfa (prueba de alfa a 0.5; los mipmaps conservan la cobertura), rocas, tocon, tronco y el
// suelo (tres texturas mezcladas por ruido con una segunda escala contra la repeticion). Normales de las texturas
// sin tangentes guardadas (marco cotangente por derivadas, Schuler 2013). Luz: sol calido con las sombras del mapa
// y moteado de las copas, cielo/rebote hemisferico con oclusion (vertice x textura), brillo segun la rugosidad y,
// en el follaje, luz que atraviesa las hojas (contraluz). Niebla con la distancia que toma el color del sol al
// mirar hacia el. El suelo y las raices crecen por fragmento con la onda; al retirarse todo se disuelve con un
// borde de brasas verdes. Lo que queda entre la camara y el tablero se abre (tramado) para no tapar el duelo.
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
layout (set = 0, binding = 1) uniform sampler2DShadow mapaSombra;
layout (set = 1, binding = 0) uniform sampler2DArray texturas;
layout (push_constant) uniform Empuje
{
	vec4 modo;       // x: 0 instancias, 1 raices, 2 suelo, 3 cesped
} empuje;

layout (location = 0) in vec3 inPosMundo;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec4 inInfo;    // material, crecido (-1: por fragmento), retirado, semilla / recorrido
layout (location = 4) in vec4 inUV;      // uv, capa de color (< 0: sin textura), oclusion

layout (location = 0) out vec4 outColor;

const vec3 NIEBLA_BOSQUE = vec3(0.40, 0.50, 0.38);   // la misma de escena.frag y del fondo (bosque_cielo.frag)
// capas del arreglo (bosque.h)
const float C_SUELO = 12.0, C_SUELO2 = 14.0, C_MUSGO = 16.0;

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

float sombra(vec3 posMundo, float nl)
{
	vec4 p = ubo.luz * vec4(posMundo, 1.0);
	p.xyz /= p.w;
	vec2 uv = p.xy * 0.5 + 0.5;
	if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || p.z > 1.0) {
		return 1.0;
	}
	float sesgo = mix(0.002, 0.0006, nl);
	vec2 texel = 1.0 / vec2(textureSize(mapaSombra, 0));
	float suma = 0.0;
	for (int y = -1; y <= 1; y += 2) {
		for (int x = -1; x <= 1; x += 2) {
			suma += texture(mapaSombra, vec3(uv + vec2(x, y) * texel * 1.2, p.z - sesgo));
		}
	}
	return suma * 0.25;
}

// manchas de sol que pasan entre las copas (0..1), se corren despacio con el viento
float moteado(vec3 p)
{
	vec2 q = p.xz * 0.22 + ubo.pantalla.z * vec2(0.04, 0.025);
	float n = ruido(vec3(q, ubo.pantalla.z * 0.06)) * 0.6 + ruido(vec3(q * 2.9 + 7.1, ubo.pantalla.z * 0.1)) * 0.4;
	return smoothstep(0.38, 0.66, n);
}

float niebla(vec3 p)
{
	float dist = length(ubo.camara.xyz - p);
	float f = 1.0 - exp(-max(dist - 18.0, 0.0) * 0.032);
	f *= mix(1.0, 0.55, clamp(p.y / 12.0, 0.0, 1.0));
	f = max(f, smoothstep(54.0, 72.0, dist));
	return f * ubo.bosque.x;
}

// 4x4 de Bayer (0..1) para el tramado
float bayer(vec2 f)
{
	ivec2 q = ivec2(mod(f, 4.0));
	int m[16] = int[](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
	return (float(m[q.y * 4 + q.x]) + 0.5) / 16.0;
}

vec3 lineal(vec3 c)
{
	return pow(c, vec3(2.2));
}

// marco tangente por derivadas (sin tangentes en los vertices)
mat3 cotangente(vec3 N, vec3 p, vec2 uv)
{
	vec3 dp1 = dFdx(p), dp2 = dFdy(p);
	vec2 duv1 = dFdx(uv), duv2 = dFdy(uv);
	vec3 dp2perp = cross(dp2, N), dp1perp = cross(N, dp1);
	vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
	float k = inversesqrt(max(max(dot(T, T), dot(B, B)), 1e-20));
	return mat3(T * k, B * k, N);
}

// normal de textura (formato GL: verde hacia arriba de la imagen; aqui la v crece hacia abajo)
vec3 normalTextura(vec4 t)
{
	vec3 n = t.xyz * 2.0 - 1.0;
	n.y = -n.y;
	return n;
}

// el suelo del bosque: tierra con ramitas y pasto ralo, hojarasca y musgo, mezclados por ruido; cada textura se
// mezcla con ella misma a otra escala y girada para que no se note la repeticion
void suelo(vec3 p, out vec3 albedo, out vec3 nt, out float rug, out float ao)
{
	vec2 uv = p.xz / 4.2;
	vec2 uv2 = mat2(0.8, -0.6, 0.6, 0.8) * p.xz / 9.7 + 0.37;
	float n1 = ruido(vec3(p.xz * 0.11, 1.5)) * 0.65 + ruido(vec3(p.xz * 0.37, 4.5)) * 0.35;
	float n2 = ruido(vec3(p.xz * 0.09, 7.5)) * 0.6 + ruido(vec3(p.xz * 0.45, 2.1)) * 0.4;
	float dMesa = length(max(abs(p.xz) - vec2(13.6, 10.3), 0.0));
	float wHojas = smoothstep(0.45, 0.62, n1);
	float wMusgo = max(smoothstep(0.55, 0.72, n2) * 0.85, (1.0 - smoothstep(0.2, 1.6, dMesa)) * 0.9);
	vec4 c0 = mix(texture(texturas, vec3(uv, C_SUELO)), texture(texturas, vec3(uv2, C_SUELO)), 0.35);
	vec4 c1 = mix(texture(texturas, vec3(uv * 1.1, C_SUELO2)), texture(texturas, vec3(uv2 * 1.3, C_SUELO2)), 0.3);
	vec4 c2 = texture(texturas, vec3(uv * 1.6, C_MUSGO));
	vec4 m0 = texture(texturas, vec3(uv, C_SUELO + 1.0));
	vec4 m1 = texture(texturas, vec3(uv * 1.1, C_SUELO2 + 1.0));
	vec4 m2 = texture(texturas, vec3(uv * 1.6, C_MUSGO + 1.0));
	vec4 c = mix(mix(c0, c1, wHojas), c2, wMusgo);
	vec4 m = mix(mix(m0, m1, wHojas), m2, wMusgo);
	albedo = lineal(c.rgb) * (0.78 + 0.44 * ruido(vec3(p.xz * 0.05, 3.3)));
	albedo *= mix(vec3(1.0), vec3(0.85, 1.0, 0.8), wMusgo * 0.5);
	nt = normalTextura(m);
	rug = m.a;
	ao = c.a;
}

void main()
{
	float mat = inInfo.x;
	float g = inInfo.y, r = inInfo.z;
	vec3 p = inPosMundo;
	float d = length(p.xz - ubo.bosque2.xy);
	float frente = 0.0;
	bool esSuelo = empuje.modo.x > 1.5 && empuje.modo.x < 2.5;
	bool cesped = empuje.modo.x > 2.5;
	if (g < 0.0) {
		// suelo y raices: la onda por fragmento, con el borde irregular
		float j = ruido(vec3(p.xz * 0.4, 1.3)) * 3.0;
		float rampa = empuje.modo.x > 1.5 ? 2.0 : 7.0;
		g = clamp((ubo.bosque.y - d - j) / rampa, 0.0, 1.0);
		r = ubo.bosque.z < 0.0 ? 0.0 : clamp((ubo.bosque.z - d - j) / rampa, 0.0, 1.0);
		frente = exp(-abs(ubo.bosque.y - d - j) * 1.2) * (1.0 - r);
		if (ubo.bosque.z >= 0.0) {
			frente += exp(-abs(ubo.bosque.z - d - j) * 1.2);
		}
		frente *= ubo.bosque.w;
		if (empuje.modo.x > 1.5) {
			if (g <= 0.02 || r >= 0.98) {
				discard;   // suelo todavia sin cubrir (o ya recogido)
			}
		} else {
			// raices: avanzan desde su base (recorrido 0) hasta la punta (1) y se recogen al reves
			float rec = inInfo.w;
			if (rec > g * 1.02 || rec > 1.0 - r) {
				discard;
			}
			frente += exp(-abs(g - rec) * 25.0) * (g < 1.0 ? 1.0 : 0.0) + exp(-abs(1.0 - r - rec) * 25.0) * (r > 0.0 ? 1.0 : 0.0);
			r = 0.0;
		}
	}
	// disuelto al retirarse (con un borde de brasas verdes)
	float patron = 1.0;
	if (r > 0.0 && empuje.modo.x < 1.5) {
		patron = ruido(p * 2.3) * 0.7 + ruido(p * 7.0) * 0.3;
		if (patron < r * 1.1) {
			discard;
		}
	}
	// los arboles entre la camara y el tablero: tramado (nunca tapan el duelo)
	if (empuje.modo.x < 0.5) {
		vec3 cam = ubo.camara.xyz;
		vec3 ab = vec3(0.0, 0.6, -0.8) - cam;
		float h = clamp(dot(p - cam, ab) / dot(ab, ab), 0.0, 1.0);
		float lejos = length(p - (cam + ab * h));
		float abrir = (1.0 - smoothstep(0.75, 0.95, h)) * (1.0 - smoothstep(mix(3.0, 13.0, h), mix(4.5, 16.0, h), lejos));
		abrir = max(abrir, 1.0 - smoothstep(4.0, 7.0, length(p - cam)));
		if (abrir > 0.0 && bayer(gl_FragCoord.xy) < abrir) {
			discard;
		}
	}

	bool hoja = mat > 0.5 && mat < 1.5;
	vec3 v = normalize(ubo.camara.xyz - p);
	vec3 n = normalize(inNormal);
	float capa = inUV.z;
	vec3 albedo;
	float rug = 0.85, ao = inUV.w;
	vec3 nt = vec3(0.0, 0.0, 1.0);
	bool conTextura = false;
	if (esSuelo) {
		float aoTex;
		suelo(p, albedo, nt, rug, aoTex);
		ao *= aoTex;
		n = vec3(0.0, 1.0, 0.0);
		nt = normalize(vec3(nt.x, nt.z, nt.y));   // uv = (x, z): tangente +x, bitangente +z
		n = normalize(mix(n, nt, 0.85));
	} else if (cesped || capa < 0.0) {
		albedo = lineal(inColor.rgb);   // cesped y hongos: color propio
		if (!cesped && dot(n, v) < 0.0) {
			n = -n;
		}
	} else {
		vec4 c = texture(texturas, vec3(inUV.xy, capa));
		if (hoja && c.a < 0.5) {
			discard;
		}
		vec4 m = texture(texturas, vec3(inUV.xy, capa + 1.0));
		albedo = lineal(c.rgb) * inColor.rgb;
		if (!hoja) {
			ao *= mix(1.0, c.a, 0.8);   // oclusion de la textura (corteza, rocas, tronco)
		}
		rug = m.a;
		conTextura = true;
		vec3 nGeo = n;
		if (!hoja && dot(nGeo, v) < 0.0) {
			nGeo = -nGeo;
		}
		mat3 tbn = cotangente(nGeo, p, inUV.xy);
		vec3 nd = normalTextura(m);
		n = normalize(tbn * vec3(nd.xy * (hoja ? 0.6 : 1.0), nd.z));
		if (!hoja) {
			// musgo encima de lo que mira hacia arriba (rocas, tocones, troncos caidos, raices, la base de los arboles)
			float arriba = smoothstep(0.35, 0.85, n.y) * smoothstep(0.35, 0.65, ruido(p * 1.3));
			arriba = max(arriba, smoothstep(0.55, 0.9, nGeo.y) * 0.6);
			vec4 cm = texture(texturas, vec3(p.xz / 1.7 + p.y * 0.3, C_MUSGO));
			albedo = mix(albedo, lineal(cm.rgb) * 0.9, arriba * 0.85);
			rug = mix(rug, 0.95, arriba);
		}
	}
	if (mat > 1.5 && mat < 2.5) {
		// sombrero del hongo: algo de veteado
		albedo *= 0.85 + 0.3 * ruido(p * 18.0);
		rug = 0.6;
	}

	vec3 l = normalize(ubo.haciaSol.xyz);
	float nl = dot(n, l);
	float s = sombra(p, max(nl, 0.0));
	float mot = 0.28 + 0.95 * moteado(p);
	vec3 colorSol = vec3(1.0, 0.90, 0.70) * 2.5;
	float difusa = hoja || cesped ? pow(clamp(nl * 0.6 + 0.4, 0.0, 1.0), 1.5) : max(nl, 0.0);
	vec3 sol = colorSol * difusa * s * mot;
	// cielo azul verdoso que baja entre las copas y rebote calido y oscuro del suelo; oclusion
	vec3 cielo = vec3(0.20, 0.27, 0.27), rebote = vec3(0.055, 0.05, 0.03);
	vec3 relleno = mix(rebote, cielo, n.y * 0.5 + 0.5) * ao;
	vec3 color = albedo * (sol + relleno);
	// brillo: segun la rugosidad (hojas enceradas, corteza humeda)
	vec3 hm = normalize(l + v);
	float brillo = pow(max(dot(n, hm), 0.0), mix(64.0, 6.0, rug)) * (1.0 - rug) * 0.6 * max(nl, 0.0) * s * mot;
	color += colorSol * brillo * 0.25;
	if (hoja || cesped) {
		// luz que atraviesa las hojas: mas fuerte mirando hacia el sol, teñida de verde amarillo
		float atraviesa = pow(max(dot(-v, l), 0.0), 4.0) * 1.2 + 0.18 * max(-nl, 0.0);
		color += albedo * vec3(1.05, 1.15, 0.45) * atraviesa * s * mot * 1.6 * ao;
	}
	if (cesped) {
		color *= 0.55 + 0.45 * inInfo.w;   // las briznas oscurecen hacia la raiz
	}
	// brasas del borde que se disuelve y la onda que pasa
	if (r > 0.0 && empuje.modo.x < 1.5) {
		color += (1.0 - smoothstep(0.0, 0.08, patron - r * 1.1)) * vec3(1.4, 2.6, 0.5);
	}
	color += vec3(0.55, 1.6, 0.35) * frente * 1.6;
	// tono filmico, vineta y gamma como escena.frag; luego el tono del bosque y la niebla
	color = vec3(1.0) - exp(-color * 1.15);
	vec2 q = gl_FragCoord.xy / ubo.pantalla.xy - 0.5;
	color *= 1.0 - 0.35 * dot(q, q) * 2.0;
	color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / 2.2));
	color *= vec3(0.96, 1.01, 0.92);
	// la niebla se tiñe del sol al mirar hacia el (luz que se dispersa en el aire)
	float haciaSol = pow(max(dot(-v, l), 0.0), 6.0);
	vec3 colNiebla = mix(NIEBLA_BOSQUE, vec3(0.78, 0.74, 0.55), haciaSol * 0.55);
	color = mix(color, colNiebla, niebla(p));
	outColor = vec4(color, 1.0);
}
