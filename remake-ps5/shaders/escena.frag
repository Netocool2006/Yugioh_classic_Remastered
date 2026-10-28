#version 450
// Yugi Reload - textura de color, sol calido con sombras suaves, cielo frio de relleno, brillo
// especular y, en los monstruos, un contorno holografico que late. Se ilumina en espacio lineal:
// la textura y la pantalla son UNORM con valores sRGB, asi que se decodifica al entrar y se
// codifica al salir.
layout (set = 0, binding = 0) uniform UBO
{
	mat4 proyeccion;
	mat4 vista;
	mat4 luz;
	vec4 haciaSol;
	vec4 camara;
	vec4 pantalla;   // ancho, alto, tiempo, destello
	vec4 bosque;     // carta de campo Bosque: x ambiente 0..1, y frente de crecimiento (m), z frente de retiro (m, < 0 nada), w onda
	vec4 bosque2;    // xy: origen de la onda (x, z del mundo: la carta de campo)
	// entorno del rival (rama classic-entornos, entornos.h): x peso visible (0: la escena queda como siempre)
	vec4 ent;
	vec4 entCieloA;
	vec4 entCieloH;
	vec4 entNiebla;  // rgb: niebla (pantalla), a: densidad (1/m)
	vec4 entSolDir;
	vec4 entSolColor;
	vec4 entLuz;     // rgb: luz del sol (lineal)
	vec4 entRelleno; // rgb: relleno del cielo (lineal), a: metros sin niebla
	vec4 entRebote;  // rgb: rebote del suelo (lineal)
	vec4 entEfecto;
} ubo;
layout (push_constant) uniform Empuje
{
	mat4 modelo;
	vec4 efecto;     // x: holograma, y: especular, z: desintegracion, w: emisivo
	vec4 vida;       // x, y: base en el mundo (x, z), z: respiracion, w: pelo
	vec4 ojoA;       // ojo con parpado (coordenadas del modelo): centro, radio (0: sin parpados)
	vec4 ojoB;       // el otro ojo: centro, cuanto esta cerrado (0..1)
} empuje;
layout (set = 0, binding = 1) uniform sampler2DShadow mapaSombra;
layout (set = 1, binding = 0) uniform sampler2D colorBase;

layout (location = 0) in vec3 inPosMundo;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inPosModelo;

layout (location = 0) out vec4 outColor;

float azar(vec3 p)
{
	return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

// ruido de valor 3D suave (0..1)
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
	float sesgo = mix(0.0015, 0.0004, nl);
	vec2 texel = 1.0 / vec2(textureSize(mapaSombra, 0));
	float suma = 0.0;
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			suma += texture(mapaSombra, vec3(uv + vec2(x, y) * texel * 1.5, p.z - sesgo));
		}
	}
	return suma / 9.0;
}

// ---- carta de campo Bosque (rama campo-bosque). Todo depende de ubo.bosque: con x = 0 y sin frentes
// (y, z < 0) no cambia nada y la escena queda EXACTAMENTE como sin el bosque
const vec3 NIEBLA_BOSQUE = vec3(0.40, 0.50, 0.38);   // color de la niebla (pantalla, ya con gamma): el mismo del fondo

// cuanto cubre el bosque el punto p (0 nada .. 1 entero): la onda de crecimiento que sale de la carta de campo menos
// la de retiro, con el borde irregular; "frente": el brillo de la onda que pasa por ahi
float coberturaBosque(vec3 p, out float frente)
{
	float d = length(p.xz - ubo.bosque2.xy);
	float j = ruido(vec3(p.xz * 0.45, 3.1)) * 2.6 + ruido(vec3(p.xz * 1.7, 7.3)) * 0.8;
	float g = clamp((ubo.bosque.y - d - j) / 1.6, 0.0, 1.0);
	float r = ubo.bosque.z < 0.0 ? 0.0 : clamp((ubo.bosque.z - d - j) / 1.6, 0.0, 1.0);
	frente = exp(-abs(ubo.bosque.y - d - j) * 1.3) * (1.0 - r);
	if (ubo.bosque.z >= 0.0) {
		frente += exp(-abs(ubo.bosque.z - d - j) * 1.3);
	}
	frente *= ubo.bosque.w;
	return g * (1.0 - r);
}

// el tapete cubierto: musgo y cesped corto, hojarasca, raices y tierra (albedo lineal)
vec3 musgoBosque(vec3 p)
{
	float n1 = ruido(vec3(p.xz * 1.3, 0.5));
	float n2 = ruido(vec3(p.xz * 4.1, 2.5));
	float n3 = ruido(vec3(p.xz * 13.0, 5.5));
	vec3 c = mix(vec3(0.03, 0.075, 0.016), vec3(0.11, 0.21, 0.035), n1 * 0.6 + n2 * 0.4);
	c *= 0.72 + 0.56 * n3;
	// briznas: rayitas finas y claras
	float brizna = smoothstep(0.75, 0.95, ruido(vec3(p.x * 38.0, p.z * 9.0, 1.0)));
	c = mix(c, vec3(0.26, 0.42, 0.08), brizna * 0.5);
	// hojarasca: manchas de hojas secas (naranja, ocre)
	float h = smoothstep(0.60, 0.70, ruido(vec3(p.xz * 0.9, 9.0))) * smoothstep(0.35, 0.6, n3);
	c = mix(c, mix(vec3(0.34, 0.11, 0.02), vec3(0.46, 0.29, 0.04), n2), h * 0.85);
	// tierra entre el musgo
	c = mix(c, vec3(0.10, 0.06, 0.03), smoothstep(0.66, 0.8, ruido(vec3(p.xz * 0.55, 14.0))) * 0.7);
	// raices: crestas oscuras de ruido que serpentean
	float rv = 1.0 - abs(ruido(vec3(p.xz * 0.7, 11.0)) * 2.0 - 1.0);
	c = mix(c, vec3(0.07, 0.04, 0.02), smoothstep(0.955, 0.99, rv) * 0.7 * smoothstep(0.4, 0.6, ruido(vec3(p.xz * 0.3, 21.0))));
	return c;
}

// luz moteada: agujeros de luz entre las copas que se mueven despacio (0..1)
float moteadoBosque(vec3 p)
{
	vec2 q = p.xz * 0.24 + ubo.pantalla.z * vec2(0.05, 0.03);
	float n = ruido(vec3(q, ubo.pantalla.z * 0.07)) * 0.65 + ruido(vec3(q * 2.7 + 7.1, ubo.pantalla.z * 0.11)) * 0.35;
	return smoothstep(0.36, 0.62, n);
}

// niebla verdosa (0..1): con la distancia y mas espesa abajo
float nieblaBosque(vec3 p)
{
	float dist = length(ubo.camara.xyz - p);
	float f = 1.0 - exp(-max(dist - 22.0, 0.0) * 0.028);
	f *= mix(1.0, 0.6, clamp(p.y / 9.0, 0.0, 1.0));
	f = max(f, smoothstep(56.0, 74.0, dist));
	return f * ubo.bosque.x;
}

void main()
{
	// desintegracion: se borran los puntos bajo el umbral; cerca del umbral, un borde encendido. Negativa: el monstruo
	// se MATERIALIZA (invocacion, haz de luz): el mismo patron, con el borde frio (cian) en vez de brasa
	float dis = abs(empuje.efecto.z);
	float patron = ruido(inPosMundo * 5.0) * 0.7 + ruido(inPosMundo * 13.0) * 0.3;
	if (dis > 0.0 && patron < dis * 1.05) {
		discard;
	}
	vec3 n = normalize(inNormal);
	vec3 v = normalize(ubo.camara.xyz - inPosMundo);
	if (dot(n, v) < 0.0) {
		n = -n; // las dos caras de las superficies finas
	}
	vec3 l = normalize(ubo.haciaSol.xyz);
	vec3 albedo = pow(texture(colorBase, inUV).rgb, vec3(2.2));
	// parpados: una piel del color del pelo baja sobre cada ojo desde arriba, con el borde oscuro y curvo
	if (empuje.ojoA.w > 0.0 && empuje.ojoB.w > 0.0) {
		float r = empuje.ojoA.w;
		for (int k = 0; k < 2; k++) {
			vec3 c = k == 0 ? empuje.ojoA.xyz : empuje.ojoB.xyz;
			vec3 d = inPosModelo - c;
			if (length(d) < r) {
				float x = d.x / r;
				float borde = r * (1.0 - 2.05 * empuje.ojoB.w) - 0.25 * r * x * x;
				if (d.y > borde) {
					// piel con pelo: mechones finos verticales (ruido estirado) y algo de variacion
					float pelo = ruido(vec3(inPosModelo.x * 90.0, inPosModelo.y * 14.0, inPosModelo.z * 90.0)) * 0.6 + ruido(inPosModelo * 35.0) * 0.4;
					albedo = pow(vec3(0.40, 0.27, 0.18) * (0.55 + 0.75 * pelo), vec3(2.2));
				} else if (d.y > borde - 0.018) {
					albedo = pow(vec3(0.12, 0.07, 0.05), vec3(2.2));
				}
			}
		}
	}

	// Bosque: el tapete (efecto.x < 0) se cubre de musgo, cesped, hojarasca y raices; los marcos de las casillas
	// (lo claro de la textura) siguen a la vista y brillan
	vec3 emisionBosque = vec3(0.0);
	if (empuje.efecto.x < 0.0 && ubo.bosque.y > 0.0 && inPosMundo.y < 0.012 && n.y > 0.5) {
		float frente;
		float cubre = coberturaBosque(inPosMundo, frente);
		float lum = dot(albedo, vec3(0.3, 0.59, 0.11));
		float marco = smoothstep(0.03, 0.11, lum);
		albedo = mix(albedo, mix(musgoBosque(inPosMundo), albedo, marco), cubre);
		float late = 0.75 + 0.25 * sin(ubo.pantalla.z * 1.6 + inPosMundo.x * 0.3);
		emisionBosque = (albedo * marco * vec3(1.2, 2.6, 0.7) * 0.55 * late) * cubre * ubo.bosque.x;
		emisionBosque += vec3(0.55, 1.6, 0.35) * frente * 1.4;
	}

	float nl = max(dot(n, l), 0.0);
	float s = sombra(inPosMundo, nl);
	vec3 sol = vec3(1.0, 0.93, 0.82) * 2.4 * nl * s;
	// relleno: cielo frio desde arriba, rebote calido de la mesa desde abajo
	vec3 relleno = mix(vec3(0.10, 0.08, 0.07), vec3(0.32, 0.38, 0.52), n.y * 0.5 + 0.5);
	if (ubo.ent.x > 0.0) {
		// la luz del entorno del rival (atardecer, noche, antorchas, Oricalco...)
		sol = mix(sol, ubo.entLuz.rgb * nl * s, ubo.ent.x);
		relleno = mix(relleno, mix(ubo.entRebote.rgb, ubo.entRelleno.rgb, n.y * 0.5 + 0.5), ubo.ent.x);
	}
	if (ubo.bosque.x > 0.0) {
		// bajo las copas: sol dorado y moteado, relleno verde del follaje y rebote del musgo
		float b = ubo.bosque.x;
		sol *= mix(vec3(1.0), vec3(1.05, 0.98, 0.72) * (0.32 + 0.9 * moteadoBosque(inPosMundo)), b);
		relleno = mix(relleno, mix(vec3(0.035, 0.06, 0.02), vec3(0.16, 0.27, 0.15), n.y * 0.5 + 0.5), b);
	}
	float brillo = pow(max(dot(n, normalize(l + v)), 0.0), 48.0) * empuje.efecto.y * nl * s;
	vec3 color = albedo * (sol + relleno) + brillo * vec3(1.0, 0.95, 0.85);

	// contorno holografico de los monstruos (azul cian), late despacio
	if (empuje.efecto.x > 0.0) {
		float borde = pow(1.0 - max(dot(n, v), 0.0), 3.0);
		float latido = 0.75 + 0.25 * sin(ubo.pantalla.z * 2.2);
		color += borde * latido * empuje.efecto.x * vec3(0.25, 0.65, 1.0);
	}

	if (dis > 0.0) {
		float borde = 1.0 - smoothstep(0.0, 0.08, patron - dis * 1.05);
		color += borde * (empuje.efecto.z < 0.0 ? vec3(0.7, 2.0, 3.4) : vec3(3.0, 1.2, 0.3));
	}
	// la esfera de energia: brilla por si misma
	if (empuje.efecto.w > 0.0 && empuje.vida.w > 0.5) {
		// resplandor de efectos (mezcla aditiva): brilla en el centro y se desvanece hacia los bordes
		float centro = pow(abs(dot(normalize(inNormal), v)), 1.6);
		color = empuje.vida.xyz * empuje.efecto.w * centro;
	} else if (empuje.efecto.w > 0.0) {
		vec3 tinte = dot(empuje.vida.xyz, empuje.vida.xyz) > 0.0 ? empuje.vida.xyz : vec3(1.0);
		color = (albedo * 2.2 * empuje.efecto.w + albedo * pow(1.0 - max(dot(n, v), 0.0), 2.0) * 2.0) * tinte;
	}
	color += emisionBosque;
	// destello del impacto
	color += vec3(1.0, 0.85, 1.0) * ubo.pantalla.w;

	// tono filmico suave y viñeta
	color = vec3(1.0) - exp(-color * 1.15);
	vec2 q = gl_FragCoord.xy / ubo.pantalla.xy - 0.5;
	color *= 1.0 - 0.35 * dot(q, q) * 2.0;
	// gamma: los efectos tenues (polvo, humo; brillo < 0.2) salen casi lineales, si no la gamma los vuelve
	// manchas blancas al sumarse; los rayos y destellos (brillo >= 0.2) quedan como siempre
	float gamma = 1.0 / 2.2;
	if (empuje.efecto.w > 0.0 && empuje.vida.w > 0.5) {
		gamma = mix(1.0, 1.0 / 2.2, smoothstep(0.04, 0.2, empuje.efecto.w));
	}
	outColor = vec4(pow(clamp(color, 0.0, 1.0), vec3(gamma)), 1.0);
	if (ubo.ent.x > 0.0 && empuje.efecto.w <= 0.0) {
		// niebla del entorno del rival (la misma de su suelo y su fondo)
		float d = length(ubo.camara.xyz - inPosMundo);
		float f = (1.0 - exp(-max(d - ubo.entRelleno.a, 0.0) * ubo.entNiebla.a)) * ubo.ent.x;
		outColor.rgb = mix(outColor.rgb, ubo.entNiebla.rgb, f);
	}
	if (ubo.bosque.x > 0.0 && empuje.efecto.w <= 0.0) {
		// tono del bosque (un poco mas verde y calido) y niebla (solo lo opaco: los efectos suman luz)
		outColor.rgb *= mix(vec3(1.0), vec3(0.94, 1.03, 0.88), ubo.bosque.x);
		outColor.rgb = mix(outColor.rgb, NIEBLA_BOSQUE, nieblaBosque(inPosMundo));
	}
}
