#version 450
// Yugi Reload - carta de campo Bosque: arboles, abetos, arbustos, helechos, hongos, rocas, tocones, troncos,
// raices y el suelo del bosque (bosque.h) dibujados por instancias. Cada instancia brota cuando la onda que sale
// de la carta de campo llega a su lugar (crece con un rebote) y, al destruirse la carta, se hunde en el suelo y se
// deshace (el fragmento la disuelve). Viento en tres capas: el arbol entero se mece (lento, mas arriba), las ramas
// se balancean (color.a: flexibilidad) y las hojas tiemblan (rapido, chico, solo el follaje).
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
layout (push_constant) uniform Empuje
{
	vec4 modo;       // x: 0 instancias, 1 raices (una malla en el mundo, crecen a lo largo), 2 suelo; y: 1 = mapa de sombras
} empuje;

layout (location = 0) in vec4 inPos;      // xyz; w: material (0 corteza, 1 follaje, 2 hongo, 3 roca, 5 suelo, 6 raiz)
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;    // rgb tinte (o albedo del hongo); a: flexibilidad (o recorrido en las raices)
layout (location = 3) in vec4 inA;        // instancia: lugar (x, y, z) y escala
layout (location = 4) in vec4 inB;        // instancia: giro (rad), semilla 0..1, estirar en altura, cuanto se hunde (m)
layout (location = 5) in vec4 inUV;       // uv, capa de color (normal = capa + 1), oclusion

layout (location = 0) out vec3 outPosMundo;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec4 outInfo;   // material, crecido 0..1 (-1: por fragmento), retirado 0..1, semilla / recorrido
layout (location = 4) out vec4 outUV;

const float RAMPA = 12.0;   // metros de onda que tarda en crecer del todo (a 22 m/s: ~0.55 s)

float rebote(float x)   // 0..1 con un sobrepaso al final (brota y se asienta)
{
	float c1 = 1.9, c3 = c1 + 1.0;
	float y = x - 1.0;
	return 1.0 + c3 * y * y * y + c1 * y * y;
}

void main()
{
	vec3 pos = inPos.xyz;
	vec3 nor = inNormal;
	float mat = inPos.w;
	float g = 1.0, r = 0.0;
	vec3 mundo;
	if (empuje.modo.x < 0.5) {
		float d = length(inA.xz - ubo.bosque2.xy);
		float jit = inB.y * 5.0;
		g = clamp((ubo.bosque.y - d - jit) / RAMPA, 0.0, 1.0);
		r = ubo.bosque.z < 0.0 ? 0.0 : clamp((ubo.bosque.z - d - jit) / (RAMPA * 0.8), 0.0, 1.0);
		float e = rebote(g);
		vec3 esc = inA.w * vec3(mix(0.25, 1.0, e), e * inB.z, mix(0.25, 1.0, e));
		float t = ubo.pantalla.z;
		float s = inB.y * 40.0;
		float viento = ubo.bosque2.z;
		float c = cos(inB.x), sn = sin(inB.x);
		vec3 p = pos * esc;
		p = vec3(c * p.x + sn * p.z, p.y, -sn * p.x + c * p.z);
		nor = normalize(vec3(c * nor.x + sn * nor.z, nor.y, -sn * nor.x + c * nor.z) / max(esc / inA.w, vec3(0.05)));
		// 1) el arbol entero: un vaiven lento con rafagas (cuadratico con la altura, el pie no se mueve)
		float h = max(p.y, 0.0) / max(inA.w * 8.0, 1.0);
		float rafaga = 0.6 + 0.4 * sin(t * 0.37 + inA.x * 0.05 + inA.z * 0.04);
		vec2 tronco = vec2(sin(t * 0.8 + s), sin(t * 0.63 + s * 1.3) * 0.6) * rafaga * h * h * 0.35 * viento;
		// 2) ramas: segun la flexibilidad del vertice, con la fase del lugar (cada rama a su ritmo)
		float fase = dot(pos, vec3(0.7, 0.3, 0.5)) + s;
		vec2 rama = vec2(sin(t * 1.7 + fase) * 0.6 + sin(t * 2.9 + fase * 1.7) * 0.25, sin(t * 1.3 + fase * 0.8) * 0.35) * inColor.a * 0.16 * rafaga * viento;
		p.xz += (tronco + rama) * inA.w;
		// 3) el temblor de las hojas (en la normal, rapido y chico)
		if (mat > 0.5 && mat < 1.5) {
			p += nor * sin(t * 7.0 + fase * 3.1) * 0.025 * inColor.a * inA.w * viento;
		}
		// al retirarse: se hunde (cuadratico: empieza suave) y se achica un poco
		p *= 1.0 - 0.35 * r;
		mundo = inA.xyz + p - vec3(0.0, r * r * inB.w, 0.0);
	} else {
		mundo = pos;   // raices y suelo: ya en el mundo; crecen por fragmento
		g = -1.0;
	}
	outPosMundo = mundo;
	outNormal = nor;
	outColor = inColor;
	outUV = inUV;
	outInfo = vec4(mat, g, r, empuje.modo.x > 0.5 ? inColor.a : inB.y);
	gl_Position = (empuje.modo.y > 0.5 ? ubo.luz : ubo.proyeccion * ubo.vista) * vec4(mundo, 1.0);
	if (g == 0.0 || r >= 1.0) {
		gl_Position = vec4(0.0, 0.0, -10.0, 1.0);   // sin brotar o ya hundido: triangulo recortado (no cuesta)
	}
}
