#version 450
// Yugioh Classic Remaster - cartas de campo: forma de cada particula (campos_particula.vert). Alfa premultiplicado:
// las de luz (motas, brasas, gotas, chispas, burbujas) suman luz (alfa 0); hojas, plumas, polvo y niebla tapan.
layout (location = 0) in vec2 inUV;
layout (location = 1) flat in vec4 inColor;
layout (location = 2) flat in vec4 inExtra;

layout (location = 0) out vec4 outColor;

void main()
{
	int tipo = int(inExtra.y + 0.5);
	float r = length(inUV);
	vec3 c = inColor.rgb;
	float a = inColor.a;
	if (tipo == 2 || tipo == 6) {
		// hoja / pluma: un huso con su nervio
		float forma = smoothstep(1.0, 0.85, length(inUV / vec2(0.38, 1.0)));
		float nervio = smoothstep(0.08, 0.0, abs(inUV.x)) * 0.3;
		float cub = forma * a;
		if (cub < 0.01) discard;
		outColor = vec4(c * (0.75 + 0.25 * inUV.y - nervio) * cub, cub);
	} else if (tipo == 4 || tipo == 5) {
		// polvo / niebla: mancha suave
		float cub = pow(max(1.0 - r, 0.0), 2.0) * a * (tipo == 5 ? 0.6 : 0.5);
		if (cub < 0.004) discard;
		outColor = vec4(c * cub, cub);
	} else if (tipo == 8) {
		// burbuja: un aro con brillo
		float aro = smoothstep(0.25, 0.0, abs(r - 0.75)) + smoothstep(0.2, 0.0, length(inUV - vec2(-0.3, 0.35))) * 0.8;
		if (aro * a < 0.01) discard;
		outColor = vec4(c * aro * a * 0.8, 0.0);
	} else {
		// luz: nucleo y halo (suma)
		float luz = exp(-r * r * 6.0) + exp(-r * r * 40.0) * 1.5;
		if (r > 1.0 || luz * a < 0.01) discard;
		outColor = vec4(c * luz * a, 0.0);
	}
}
