#version 450
// Yugi Reload - flipbook del Niagara Examples Pack (src/fx.h): el cuadro del atlas (dos seguidos, mezclados por la
// fraccion del tiempo) con su alfa premultiplicado. El atlas guarda el color en sRGB: aqui vuelve a lineal, se tine
// y sale con la gamma de la pantalla (como escena.frag). Mezcla ONE, ONE_MINUS_SRC_ALPHA.
layout (set = 1, binding = 0) uniform sampler2DArray atlas;

layout (location = 0) in vec2 inUV;
layout (location = 1) flat in vec4 inColor;
layout (location = 2) flat in vec4 inCuadro;   // capa, cuadro, cuadros, columnas
layout (location = 3) flat in vec2 inExtra;    // llenado, loop
layout (location = 4) flat in vec4 inRecorte;  // v desde, v hasta, borde que se funde

layout (location = 0) out vec4 outColor;

vec4 cuadro(float k, vec2 uv)
{
	float cols = inCuadro.w;
	float col = mod(k, cols);
	float fila = floor(k / cols);
	vec2 p = (vec2(col, fila) + uv) / cols * inExtra.x;
	return texture(atlas, vec3(p, inCuadro.x));
}

void main()
{
	// medio texel de margen (256 px por cuadro) para no tomar el cuadro vecino
	vec2 uv = clamp(inUV, vec2(1.0 / 256.0), vec2(1.0 - 1.0 / 256.0));
	float n = inCuadro.z;
	float k0 = floor(inCuadro.y);
	float f = inCuadro.y - k0;
	float k1 = k0 + 1.0;
	if (k1 >= n) {
		k1 = inExtra.y > 0.5 ? 0.0 : n - 1.0;
	}
	vec4 t = mix(cuadro(k0, uv), cuadro(k1, uv), f);
	// el borde del cuadro (y el del recorte) se funde a cero: lo que el pack deja tocando el borde no muestra el
	// cuadrado de la particula
	float b = max(inRecorte.z, 1e-3);
	float borde = smoothstep(0.0, b, inUV.x) * smoothstep(0.0, b, 1.0 - inUV.x) *
	              smoothstep(inRecorte.x, inRecorte.x + b, inUV.y) * smoothstep(0.0, b, inRecorte.y - inUV.y);
	t *= borde;
	vec3 lineal = pow(t.rgb, vec3(2.2));
	float a = t.a * inColor.a;
	vec3 c = lineal * inColor.rgb * inColor.a;
	// a la pantalla con gamma (la mezcla es en el espacio de la pantalla, como los demas efectos): lo opaco (humo) se
	// despremultiplica, se le pone gamma y se vuelve a premultiplicar; lo aditivo (alfa ~ 0) va con gamma directa
	vec3 g1 = pow(clamp(c, 0.0, 4.0), vec3(1.0 / 2.2));
	vec3 g2 = pow(clamp(c / max(a, 1e-3), 0.0, 4.0), vec3(1.0 / 2.2)) * a;
	outColor = vec4(min(mix(g1, g2, a), vec3(1.0)), a);
}
