#version 450
// Yugioh Classic Remaster - entornos por personaje rival: particulas sin buffers (cada instancia sale de un azar de su
// numero), repartidas en un anillo alrededor de la mesa (zona.x..zona.y metros desde su borde) para no tapar el
// tablero:
//   0 motas que flotan y brillan (luciernagas, polvo dorado, flashes)   1 chispas que suben (brasas, Oricalco)
//   2 hojas, petalos o papelitos que caen girando                       3 datos que caen (columnas verdes de Noah)
//   4 nubes que pasan con el viento (dirigible)                          5 rafagas de viento (rayas finas)
//   6 bancos de niebla bajos                                              7 humo oscuro que sube (Reino de las Sombras)
//   8 arena que vuela a ras del suelo
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
	vec4 ent;        // x: peso visible
} ubo;
layout (push_constant) uniform Empuje
{
	vec4 modo;       // x: tipo, y: peso, z: mitad del ancho de la mesa, w: mitad del fondo
	vec4 color;      // rgb, a: opacidad (< 0: papelitos de colores)
	vec4 zona;       // metros desde el borde de la mesa (min, max), altura (min, max)
} empuje;

layout (location = 0) out vec2 outUV;
layout (location = 1) flat out vec4 outColor;
layout (location = 2) flat out vec4 outExtra;   // x: semilla, y: tipo

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

// un lugar del anillo alrededor de la mesa (el rectangulo agrandado "lejos" metros)
vec2 anillo(uint i, uint k)
{
	float ang = azar(i, k) * 6.2832;
	float lejos = mix(empuje.zona.x, empuje.zona.y, azar(i, k + 1u));
	vec2 b = vec2(cos(ang), sin(ang));
	return b / max(abs(b.x) / (empuje.modo.z + lejos), abs(b.y) / (empuje.modo.w + lejos));
}

void main()
{
	vec2 q = ESQUINAS[gl_VertexIndex];
	uint i = uint(gl_InstanceIndex);
	int tipo = int(empuje.modo.x + 0.5);
	float t = ubo.pantalla.z;
	vec3 cam = ubo.camara.xyz;
	vec3 pos;
	vec3 ejeX, ejeY;
	vec4 color = vec4(empuje.color.rgb, abs(empuje.color.a));
	float alto = empuje.zona.w - empuje.zona.z;
	float tam = 0.1;
	bool mirar = true;   // de cara a la camara
	if (tipo == 0) {
		vec2 b = anillo(i, 1u);
		pos = vec3(b.x, empuje.zona.z + alto * azar(i, 4u), b.y) +
		      vec3(sin(t * 0.37 + azar(i, 5u) * 6.28) * 1.4, sin(t * 0.71 + azar(i, 6u) * 6.28) * 0.35, cos(t * 0.43 + azar(i, 7u) * 6.28) * 1.4);
		color.rgb *= 0.15 + 1.1 * pow(0.5 + 0.5 * sin(t * (1.2 + 1.6 * azar(i, 8u)) + azar(i, 9u) * 6.28), 4.0);
		tam = 0.12 + 0.1 * azar(i, 10u);
	} else if (tipo == 1 || tipo == 7) {
		// sube desde el suelo y se apaga arriba
		float periodo = (tipo == 1 ? 4.0 : 7.0) + 4.0 * azar(i, 1u);
		float u = fract(t / periodo + azar(i, 2u));
		vec2 b = anillo(i, 3u);
		pos = vec3(b.x, empuje.zona.z + alto * u, b.y) + vec3(sin(t * 1.1 + azar(i, 6u) * 6.28), 0.0, cos(t * 0.9 + azar(i, 7u) * 6.28)) * (tipo == 1 ? 0.5 : 1.2) * u;
		color.a *= smoothstep(0.0, 0.1, u) * (1.0 - smoothstep(0.6, 1.0, u));
		if (tipo == 1) {
			color.rgb *= 0.6 + 0.6 * pow(0.5 + 0.5 * sin(t * 9.0 + azar(i, 8u) * 30.0), 3.0);
			tam = 0.06 + 0.07 * azar(i, 9u);
		} else {
			tam = 1.6 + 2.2 * azar(i, 9u) * (0.5 + u);
		}
	} else if (tipo == 2) {
		// cae girando (cuadrado orientado en 3D)
		float periodo = 8.0 + 6.0 * azar(i, 1u);
		float u = fract(t / periodo + azar(i, 2u));
		vec2 b = anillo(i, 3u);
		pos = vec3(b.x, empuje.zona.w - alto * u, b.y) + vec3(sin(t * 1.3 + azar(i, 6u) * 6.28) * 0.8 + u * 2.0, 0.0, cos(t * 0.9 + azar(i, 7u) * 6.28) * 0.5);
		float a1 = t * (1.2 + 2.0 * azar(i, 8u)) + azar(i, 9u) * 6.28;
		float a2 = t * (0.8 + 1.5 * azar(i, 10u)) + azar(i, 11u) * 6.28;
		vec3 u1 = vec3(cos(a1), sin(a1) * cos(a2), sin(a1) * sin(a2));
		vec3 u2 = normalize(cross(u1, vec3(sin(a2), cos(a2), 0.3)));
		tam = 0.09 + 0.06 * azar(i, 12u);
		ejeX = u1 * tam;
		ejeY = u2 * tam * 1.4;
		mirar = false;
		if (empuje.color.a < 0.0) {   // papelitos de colores
			float k = azar(i, 13u);
			color.rgb = k < 0.25 ? vec3(1.0, 0.25, 0.3) : k < 0.5 ? vec3(0.25, 0.6, 1.0) : k < 0.75 ? vec3(1.0, 0.85, 0.2) : vec3(0.95);
			ejeY = u2 * tam * 0.6;
		} else {
			color.rgb *= 0.75 + 0.5 * azar(i, 13u);
		}
		color.rgb *= 0.55 + 0.45 * abs(dot(normalize(cross(u1, u2)), normalize(ubo.haciaSol.xyz)));
		color.a *= smoothstep(0.0, 0.05, u) * (1.0 - smoothstep(0.9, 1.0, u));
	} else if (tipo == 3) {
		// columna de datos: raya vertical que cae rapido
		float periodo = 2.5 + 3.0 * azar(i, 1u);
		float u = fract(t / periodo + azar(i, 2u));
		vec2 b = anillo(i, 3u);
		pos = vec3(b.x, empuje.zona.w - alto * u, b.y);
		vec3 aCam = normalize(cam - pos);
		ejeX = normalize(cross(vec3(0.0, 1.0, 0.0), aCam)) * 0.05;
		ejeY = vec3(0.0, 1.2 + 1.5 * azar(i, 4u), 0.0);
		mirar = false;
		color.a *= smoothstep(0.0, 0.1, u) * (1.0 - smoothstep(0.85, 1.0, u));
	} else if (tipo == 4) {
		// nube: grande y blanda, pasa con el viento a lo largo de x y vuelve a entrar
		float vel = 6.0 + 6.0 * azar(i, 1u);
		vec2 b = anillo(i, 2u);
		float x = mod(b.x + t * vel + 90.0, 180.0) - 90.0;
		pos = vec3(x, empuje.zona.z + alto * azar(i, 4u), b.y);
		tam = 6.0 + 7.0 * azar(i, 5u);
		color.a *= smoothstep(90.0, 60.0, abs(x));
	} else if (tipo == 5) {
		// rafaga: raya fina y larga en la direccion del viento (x), muy rapida
		float periodo = 1.5 + 2.0 * azar(i, 1u);
		float u = fract(t / periodo + azar(i, 2u));
		vec2 b = anillo(i, 3u);
		pos = vec3(b.x - 30.0 + 60.0 * u, empuje.zona.z + alto * azar(i, 4u), b.y);
		vec3 aCam = normalize(cam - pos);
		ejeX = vec3(2.5 + 2.0 * azar(i, 5u), 0.0, 0.0);
		ejeY = normalize(cross(aCam, ejeX)) * 0.025;
		mirar = false;
		color.a *= sin(u * 3.1416);
	} else if (tipo == 6) {
		// banco de niebla bajo, de pie mirando a la camara
		vec2 b = anillo(i, 1u) + vec2(sin(t * 0.05 + azar(i, 3u) * 6.28), cos(t * 0.04 + azar(i, 4u) * 6.28)) * 2.5;
		float r = 4.0 + 5.0 * azar(i, 5u);
		pos = vec3(b.x, empuje.zona.z + r * 0.35, b.y);
		vec3 aCam = cam - pos;
		aCam.y = 0.0;
		aCam = normalize(aCam + vec3(1e-4, 0.0, 0.0));
		ejeX = vec3(-aCam.z, 0.0, aCam.x) * r;
		ejeY = vec3(0.0, r * 0.38, 0.0);
		mirar = false;
		color.a *= smoothstep(5.0, 12.0, length(cam - pos));
		tam = r;
	} else {
		// arena: granos que vuelan a ras del suelo
		float periodo = 2.0 + 2.0 * azar(i, 1u);
		float u = fract(t / periodo + azar(i, 2u));
		vec2 b = anillo(i, 3u);
		pos = vec3(b.x - 8.0 + 16.0 * u, empuje.zona.z + alto * azar(i, 4u) + sin(u * 9.0 + azar(i, 5u) * 6.0) * 0.2, b.y);
		tam = 0.03 + 0.03 * azar(i, 6u);
		color.a *= sin(u * 3.1416);
	}
	if (mirar) {
		vec3 aCam = normalize(cam - pos);
		ejeX = normalize(cross(vec3(0.0, 1.0, 0.0), aCam)) * tam;
		ejeY = normalize(cross(aCam, ejeX)) * tam;
	}
	color.a *= empuje.modo.y;
	vec3 p = pos + ejeX * q.x + ejeY * q.y;
	outUV = q;
	outColor = color;
	outExtra = vec4(azar(i, 20u), float(tipo), tam, 0.0);
	gl_Position = ubo.proyeccion * ubo.vista * vec4(p, 1.0);
	if (color.a <= 0.002) {
		gl_Position = vec4(0.0, 0.0, -10.0, 1.0);
	}
}
