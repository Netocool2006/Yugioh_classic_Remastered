#version 450
// Yugi Reload - efectos del Niagara Examples Pack horneados a flipbook (src/fx.h): un cuadrado por instancia.
// La imagen del cuadro (lado pos.w m) se apoya en el punto de origen del sistema (extra.xy, en el cuadro: 0..1, y
// hacia abajo) y se orienta segun eje.w:
//   0 de cara a la camara, con el "arriba" del mundo proyectado (lo que sube en Unreal sube aqui)
//   1 a lo largo del eje (eje.xyz = el arriba del efecto) girando sobre el para mirar a la camara (fuego de aliento)
//   2 plano sobre la mesa (efectos horneados desde arriba)
layout (set = 0, binding = 0) uniform UBO
{
	mat4 proyeccion;
	mat4 vista;
	mat4 luz;
	vec4 haciaSol;
	vec4 camara;
	vec4 pantalla;   // ancho, alto, tiempo, destello
} ubo;

layout (location = 0) in vec4 inPos;      // origen del sistema, lado del cuadro (m)
layout (location = 1) in vec4 inColor;    // tinte, opacidad
layout (location = 2) in vec4 inEje;      // arriba del efecto, modo
layout (location = 3) in vec4 inCuadro;   // capa, cuadro (con fraccion), cuadros, columnas
layout (location = 4) in vec4 inExtra;    // origen x, y en el cuadro, llenado de la capa, loop
layout (location = 5) in vec4 inRecorte;  // v desde, v hasta, borde que se funde

layout (location = 0) out vec2 outUV;              // 0..1 dentro del cuadro (y hacia abajo)
layout (location = 1) flat out vec4 outColor;
layout (location = 2) flat out vec4 outCuadro;
layout (location = 3) flat out vec2 outExtra;      // llenado, loop
layout (location = 4) flat out vec4 outRecorte;

const vec2 ESQUINAS[6] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
                                vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

void main()
{
	vec2 uv = ESQUINAS[gl_VertexIndex];
	uv.y = mix(inRecorte.x, inRecorte.y, uv.y);   // solo la parte pedida del cuadro
	vec3 o = inPos.xyz;
	float lado = inPos.w;
	int modo = int(inEje.w + 0.5);
	vec3 aCam = normalize(ubo.camara.xyz - o);
	vec3 der, arr;
	if (modo == 2) {
		der = vec3(1.0, 0.0, 0.0);
		arr = vec3(0.0, 0.0, -1.0);
		o.y += 0.02;
	} else if (modo == 1) {
		arr = normalize(inEje.xyz);
		der = cross(arr, aCam);
		der = length(der) > 1e-3 ? normalize(der) : vec3(1.0, 0.0, 0.0);
		o += aCam * (0.12 * lado);   // que no se corte de golpe contra la mesa o el cuerpo
	} else {
		der = cross(vec3(0.0, 1.0, 0.0), aCam);
		der = length(der) > 1e-3 ? normalize(der) : vec3(1.0, 0.0, 0.0);
		arr = cross(aCam, der);
		// un poco hacia la camara: lo que queda debajo del origen (explosion en la mesa) no se corta tan duro
		o += aCam * (0.12 * lado);
	}
	vec3 pos = o + der * ((uv.x - inExtra.x) * lado) + arr * ((inExtra.y - uv.y) * lado);
	outUV = uv;
	outColor = inColor;
	outCuadro = inCuadro;
	outExtra = inExtra.zw;
	outRecorte = inRecorte;
	gl_Position = ubo.proyeccion * ubo.vista * vec4(pos, 1.0);
}
