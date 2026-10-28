#version 450
// Yugioh Classic Remaster - entornos por personaje rival (src/entornos.h): el suelo, las siluetas y las llamas del
// entorno, ya en el mundo. Al cambiar de entorno (o al entrar una carta de campo) todo se hunde en el suelo y el
// nuevo sube (ent.z: 0 hundido .. 1 en su lugar).
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
	vec4 ent;        // x: peso visible 0..1, y: entorno, z: en su lugar 0..1
} ubo;
layout (push_constant) uniform Empuje
{
	vec4 modo;       // x: 0 opaco, 1 aditivo; y: cuanto se hunde (m)
} empuje;

layout (location = 0) in vec4 inPos;      // xyz; w: material (entornos::Material)
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;    // rgb tinte; a: emision
layout (location = 3) in vec4 inUV;       // uv, capa de color (normal = capa + 1; < 0 sin textura), oclusion

layout (location = 0) out vec3 outPosMundo;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec4 outUV;
layout (location = 4) flat out float outMat;

void main()
{
	vec3 p = inPos.xyz;
	float h = 1.0 - ubo.ent.z;
	// se hunde: lo alto y lejano baja mas (se esconde detras del horizonte), el suelo apenas
	p.y -= h * h * empuje.modo.y * (inPos.w > 3.5 && inPos.w < 5.5 ? 0.15 : 1.0);
	outPosMundo = p;
	outNormal = inNormal;
	outColor = inColor;
	outUV = inUV;
	outMat = inPos.w;
	gl_Position = ubo.proyeccion * ubo.vista * vec4(p, 1.0);
}
