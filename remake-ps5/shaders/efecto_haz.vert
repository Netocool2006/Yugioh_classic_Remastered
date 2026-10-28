#version 450
// Yugi Reload - haz de invocacion (rayo de abduccion que baja del cielo y deja al monstruo en su carta).
// Un cuadrado de 2 triangulos sin vertices (sale de gl_VertexIndex) que, segun el modo (c.x):
//   0 cono:   de b (abajo, radio b.w) hasta a (arriba, radio a.w), girado sobre su eje para mirar a la camara
//             (el fragmento lo vuelve un volumen de luz: nucleo, paredes brillantes, vetas y bandas que bajan)
//   1 anillo: horizontal, centro a, radio a.w (los aros de luz que bajan por el haz, la onda del impacto)
//   2 circulo de luz sobre la mesa: centro a, radio a.w (runas que giran donde el haz toca la carta)
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
	vec4 a;          // cono: arriba (centro, radio); anillo y circulo: centro, radio
	vec4 b;          // cono: abajo (centro, radio)
	vec4 c;          // x: modo, y: semilla 0..1, z: intensidad, w: cono: hasta donde llega la luz (0 arriba .. 1 abajo)
	vec4 d;          // color de las paredes / del acento (rgb); w: anillo: grosor relativo
} haz;

layout (location = 0) out vec3 outPosMundo;
layout (location = 1) flat out vec3 outLado;

const vec2 ESQUINAS[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
                                vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

void main()
{
	vec2 q = ESQUINAS[gl_VertexIndex];
	int modo = int(haz.c.x + 0.5);
	vec3 pos;
	vec3 lado = vec3(1.0, 0.0, 0.0);
	if (modo == 0) {
		vec3 eje = haz.a.xyz - haz.b.xyz;          // de abajo hacia arriba
		float largo = max(length(eje), 1e-3);
		vec3 dir = eje / largo;
		vec3 aCam = normalize(ubo.camara.xyz - 0.5 * (haz.a.xyz + haz.b.xyz));
		lado = cross(dir, aCam);
		lado = length(lado) > 1e-3 ? normalize(lado) : vec3(1.0, 0.0, 0.0);
		float h = 0.5 * q.y + 0.5;                 // 0 abajo .. 1 arriba
		float r = mix(haz.b.w, haz.a.w, h) * 1.45; // con margen para el resplandor de afuera
		pos = haz.b.xyz + eje * h + lado * q.x * r;
	} else {
		float r = haz.a.w * (modo == 1 ? 1.25 : 1.15);
		pos = haz.a.xyz + vec3(q.x * r, 0.0, q.y * r);
	}
	outPosMundo = pos;
	outLado = lado;
	gl_Position = ubo.proyeccion * ubo.vista * vec4(pos, 1.0);
}
