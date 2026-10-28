#version 450
// Yugi Reload - posicion en el mundo (con la "vida" de los monstruos), normal y UV de cada vertice.
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
	                 // ALAS (si ojoA.w < 0; los parpados piden ojoA.w > 0): ojoA = (centro x, altura de la bisagra,
	                 // distancia de la bisagra al centro, -1 - pendiente), ojoB = (angulo de las alas en rad, + arriba;
	                 // ancho de la transicion junto a la bisagra; abajo; corte). Todo en coordenadas del modelo glTF
} empuje;

// Aleteo sin esqueleto y sin deformar la figura: cada ala (lo que queda mas alla de la bisagra, |x - centro| >
// bisagra, por encima de "abajo" y detras del plano z - pendiente * y = corte, para dejar quietos cola, patas y
// brazos) gira ENTERA como una pieza rigida alrededor de la linea de su bisagra (paralela al eje Z del modelo,
// adelante-atras); solo en los bordes de esa zona el angulo pasa suave de 0 al de la pieza, para que la malla no se
// rompa. Las dos alas suben y bajan a la vez (espejadas)
void aletear(inout vec3 p, inout vec3 n)
{
	if (empuje.ojoA.w >= 0.0 || empuje.efecto.w > 0.0) {
		return;
	}
	float dx = p.x - empuje.ojoA.x;
	float lado = dx < 0.0 ? -1.0 : 1.0;
	float d = abs(dx) - empuje.ojoA.z;
	if (d <= 0.0) {
		return;
	}
	float borde = max(empuje.ojoB.y, 1e-4);
	float pendiente = -empuje.ojoA.w - 1.0;
	float pieza = smoothstep(0.0, borde, d) * smoothstep(empuje.ojoB.z - 0.25 * borde, empuje.ojoB.z + 0.25 * borde, p.y) *
	              (1.0 - smoothstep(empuje.ojoB.w - 0.25 * borde, empuje.ojoB.w + 0.25 * borde, p.z - pendiente * p.y));
	float a = empuje.ojoB.x * lado * pieza;
	vec2 bisagra = vec2(empuje.ojoA.x + lado * empuje.ojoA.z, empuje.ojoA.y);
	float c = cos(a), s = sin(a);
	vec2 r = p.xy - bisagra;
	p.xy = bisagra + vec2(c * r.x - s * r.y, s * r.x + c * r.y);
	n.xy = vec2(c * n.x - s * n.y, s * n.x + c * n.y);
}

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
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec3 outPosMundo;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outPosModelo;

void main()
{
	vec3 pos = inPos;
	vec3 normal = inNormal;
	aletear(pos, normal);
	doblar(pos, normal);
	vec3 normalMundo = normalize(mat3(empuje.modelo) * normal);
	vec3 mundo = (empuje.modelo * vec4(pos, 1.0)).xyz;
	outPosMundo = mundo;
	outNormal = normalMundo;
	outUV = inUV;
	outPosModelo = inPos;
	gl_Position = ubo.proyeccion * ubo.vista * vec4(mundo, 1.0);
}
