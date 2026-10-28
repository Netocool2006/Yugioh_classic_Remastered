#version 450
// Yugi Reload - haz de invocacion: luz pura (mezcla aditiva, no tapa nada). Ver efecto_haz.vert.
//  color:   haz.d.rgb, el de la naturaleza del monstruo (reglas/color_haz.h); el nucleo, casi blanco con su toque
//  cono:    volumen de luz; se calcula la cuerda que atraviesa el cono (mas luz en el centro), paredes que
//           brillan mas (como un tubo de luz visto de costado), un nucleo blanco, vetas de ruido y bandas que corren
//           hacia ABAJO (el haz "deposita" al monstruo) y el frente que baja del cielo con una punta encendida
//  anillo:  aro fino con trazos que giran
//  circulo: el sello de luz donde el haz toca la mesa: relleno suave, tres aros (dos con segmentos que giran en
//           sentidos opuestos) y marcas radiales
layout (set = 0, binding = 0) uniform UBO
{
	mat4 proyeccion;
	mat4 vista;
	mat4 luz;
	vec4 haciaSol;
	vec4 camara;
	vec4 pantalla;   // ancho, alto, tiempo, destello
} ubo;
layout (push_constant) uniform Haz
{
	vec4 a;
	vec4 b;
	vec4 c;          // x: modo, y: semilla, z: intensidad, w: frente del cono
	vec4 d;          // color (rgb); w: grosor del anillo
} haz;

layout (location = 0) in vec3 inPosMundo;
layout (location = 1) flat in vec3 inLado;

layout (location = 0) out vec4 outColor;

const float PI = 3.14159265;
const vec3 BLANCO = vec3(0.92, 0.97, 1.0);
const vec3 HIELO_N = vec3(0.30, 0.78, 1.45) / 1.45;   // el color del haz original (normalizado)

// nucleo casi blanco con un toque del color del haz (el del hielo queda igual al blanco original): misma cuenta que
// nucleo_de en herramientas/color_haz.py
vec3 nucleoHaz()
{
	vec3 c = haz.d.rgb / max(max(haz.d.r, haz.d.g), max(haz.d.b, 1e-3));
	return clamp(BLANCO + 0.10 * (c - HIELO_N), 0.0, 1.0);
}

float azar(vec2 p)
{
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

float ruido(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(azar(i), azar(i + vec2(1.0, 0.0)), u.x), mix(azar(i + vec2(0.0, 1.0)), azar(i + vec2(1.0, 1.0)), u.x), u.y);
}

float fbm(vec2 p)
{
	float s = 0.0;
	float k = 0.5;
	for (int i = 0; i < 4; i++) {
		s += k * ruido(p);
		p = p * 2.03 + vec2(17.1, 9.2);
		k *= 0.5;
	}
	return s;
}

vec3 cono()
{
	float t = ubo.pantalla.z;
	float s = haz.c.y;
	vec3 eje = haz.b.xyz - haz.a.xyz;   // de arriba hacia abajo
	float largo = max(length(eje), 1e-3);
	vec3 baja = eje / largo;
	vec3 rel = inPosMundo - haz.a.xyz;
	float h = dot(rel, baja) / largo;   // 0 arriba .. 1 abajo
	float r = mix(haz.a.w, haz.b.w, clamp(h, 0.0, 1.0));
	float x = dot(rel, inLado) / max(r, 1e-3);
	float ax = abs(x);
	float y = inPosMundo.y;
	// volumen
	float cuerda = sqrt(max(1.0 - x * x, 0.0));
	float pared = smoothstep(0.55, 0.96, ax) * (1.0 - smoothstep(0.96, 1.03, ax));
	float nucleo = exp(-x * x * 16.0);
	float halo = exp(-max(ax - 1.0, 0.0) * 6.0) * (1.0 - smoothstep(1.15, 1.45, ax)) * step(1.0, ax);
	// vetas y bandas que bajan (el ruido corre con y + t: el mismo dibujo aparece cada vez mas abajo)
	float v1 = fbm(vec2(x * 3.5 + s * 23.0, (y + t * 3.4) * 0.42));
	float v2 = fbm(vec2(x * 9.0 - s * 11.0, (y + t * 5.5) * 0.95));
	float vetas = 0.45 + 1.1 * smoothstep(0.38, 0.78, v1) + 0.35 * (v2 - 0.5);
	float bandas = pow(0.5 + 0.5 * sin((y + t * 2.8) * 2.3), 18.0) + 0.5 * pow(0.5 + 0.5 * sin((y + t * 4.1) * 5.1 + 1.3), 30.0);
	float luz = (0.26 * cuerda + 0.95 * pared) * vetas + 0.75 * nucleo * (0.75 + 0.5 * v2) + 0.55 * bandas * (cuerda + pared) + 0.22 * halo;
	// a lo largo: nace suave en el cielo y crece parejo hacia la mesa (sin un escalon que lo haga ver como un bloque)
	luz *= smoothstep(0.0, 0.12, h) * (0.65 + 0.55 * h * h);
	// solo hasta el frente (baja del cielo al aparecer, sube al recogerse), con la punta encendida
	float frente = haz.c.w;
	float lleno = 1.0 - smoothstep(frente - 0.006, frente + 0.002, h);
	float punta = frente < 0.995 ? exp(-abs(h - frente) * largo * 3.0) * (1.0 - smoothstep(0.0, 0.004, h - frente)) * cuerda * 2.2 : 0.0;
	vec3 col = mix(haz.d.rgb, nucleoHaz(), clamp(0.85 * nucleo + 0.6 * bandas, 0.0, 1.0));
	return (col * luz * lleno + nucleoHaz() * punta) * haz.c.z;
}

vec3 anillo()
{
	float t = ubo.pantalla.z;
	vec2 p = (inPosMundo.xz - haz.a.xz) / max(haz.a.w, 1e-3);
	float rr = length(p);
	float g = max(haz.d.w, 0.005);
	float aro = exp(-pow((rr - 1.0) / g, 2.0));
	float brillo = exp(-abs(rr - 1.0) / (g * 5.0)) * 0.35;
	float ang = atan(p.y, p.x);
	float trazos = 0.6 + 0.4 * step(0.0, sin(ang * 18.0 + t * 4.0 + haz.c.y * 20.0));
	vec3 col = mix(haz.d.rgb, nucleoHaz(), aro * 0.65);
	return col * (aro * trazos + brillo) * haz.c.z * (1.0 - smoothstep(1.1, 1.25, rr));
}

vec3 circulo()
{
	float t = ubo.pantalla.z;
	vec2 p = (inPosMundo.xz - haz.a.xz) / max(haz.a.w, 1e-3);
	float rr = length(p);
	float ang = atan(p.y, p.x);
	float relleno = exp(-rr * rr * 2.6) * 0.55;
	float exterior = exp(-abs(rr - 1.0) * 60.0);
	float seg1 = exp(-abs(rr - 0.74) * 70.0) * step(0.35, fract(ang * 12.0 / (2.0 * PI) + t * 0.35));
	float seg2 = exp(-abs(rr - 0.52) * 80.0) * step(0.5, fract(ang * 6.0 / (2.0 * PI) - t * 0.55));
	float marcas = step(0.8, fract(ang * 48.0 / (2.0 * PI) + t * 0.12)) * step(0.82, rr) * step(rr, 0.93);
	float pulso = 0.8 + 0.2 * sin(t * 6.0);
	float luz = relleno * pulso + exterior * 0.9 + (seg1 + seg2) * 0.85 + marcas * 0.55;
	vec3 col = mix(haz.d.rgb, nucleoHaz(), clamp(relleno * 1.4, 0.0, 1.0));
	return col * luz * haz.c.z * (1.0 - smoothstep(1.02, 1.15, rr));
}

void main()
{
	int modo = int(haz.c.x + 0.5);
	vec3 col = modo == 0 ? cono() : modo == 1 ? anillo() : circulo();
	outColor = vec4(col, 0.0);   // aditivo: solo suma luz
}
