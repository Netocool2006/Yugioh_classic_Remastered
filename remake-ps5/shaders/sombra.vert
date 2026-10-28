#version 450
// Yugi Reload - profundidad vista desde el sol (mapa de sombras), con la misma "vida" que la escena.
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
	mat4 modelo;
	vec4 efecto;     // x: holograma, y: especular, z: desintegracion, w: emisivo
	vec4 vida;       // monstruos: rodilla, cintura, altura de la rodilla y de los pies (doblar); emisivo: tinte
	vec4 ojoA;       // ojo con parpado (coordenadas del modelo): centro, radio (0: sin parpados)
	vec4 ojoB;       // el otro ojo: centro, cuanto esta cerrado (0..1)
} empuje;

// DOBLAR EL CUERPO al morir, sin esqueleto (regla del usuario 2026-10-17: ninguno muere como estatua; los de piernas
// o patas las doblan). Solo para monstruos (no emisivos); vida = (rodilla, cintura, altura de la rodilla, altura de los
// pies) en coordenadas del modelo (frente +Z, arriba +Y) y efecto.w < 0 = -plegar (ver Pose en combate_api.h).
//  - plegar: el tramo de los pies a la rodilla se acorta (las patas ceden) y se abre hacia afuera; lo de arriba baja
//    entero, sin deformarse
//  - rodilla/cintura: dos bisagras rigidas alrededor del eje X: las piernas giran desde los pies (tobillo) y todo lo de
//    arriba de la rodilla gira desde la rodilla (relativo a las piernas). Solo una franja corta junto a cada bisagra
//    pasa suave de un angulo al otro (como las alas: cada tramo se mueve entero, nada de gelatina)
void girarX(inout vec3 p, inout vec3 n, float y0, float a)
{
	float c = cos(a), s = sin(a);
	vec2 r = vec2(p.y - y0, p.z);
	p.y = y0 + c * r.x - s * r.y;
	p.z = s * r.x + c * r.y;
	n.yz = vec2(c * n.y - s * n.z, s * n.y + c * n.z);
}

void doblar(inout vec3 p, inout vec3 n)
{
	float rodilla = empuje.vida.x, cintura = empuje.vida.y;
	float plegar = max(-empuje.efecto.w, 0.0);
	if (empuje.efecto.w > 0.0 || (rodilla == 0.0 && cintura == 0.0 && plegar == 0.0)) {
		return;
	}
	float pies = empuje.vida.w;
	float alto = empuje.vida.z;   // la rodilla
	float largo = max(alto - pies, 1e-3);
	float franja = 0.18 * largo;
	if (plegar > 0.0) {
		float w = smoothstep(alto - franja, alto + franja, p.y);
		float h = clamp((p.y - pies) / largo, 0.0, 1.0);
		float abajo = pies + (p.y - pies) * (1.0 - plegar);   // las patas, acortadas
		float arriba = p.y - largo * plegar;                  // lo de arriba baja entero
		p.xz += p.xz * (0.45 * plegar * (1.0 - w) * (1.0 - h));
		p.y = mix(abajo, arriba, w);
		alto -= largo * plegar;
	}
	if (rodilla != 0.0 || cintura != 0.0) {
		float wa = smoothstep(alto - franja, alto + franja, p.y);   // (los pesos, antes de girar)
		float wp = smoothstep(pies, pies + 0.25 * largo, p.y);
		girarX(p, n, alto, cintura * wa);
		girarX(p, n, pies, rodilla * wp);
	}
}

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;

void main()
{
	vec3 pos = inPos;
	vec3 normal = inNormal;
	doblar(pos, normal);
	vec3 mundo = (empuje.modelo * vec4(pos, 1.0)).xyz;
	gl_Position = ubo.luz * vec4(mundo, 1.0);
}
