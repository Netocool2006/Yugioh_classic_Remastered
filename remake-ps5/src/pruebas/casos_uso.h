// Registro de los casos de uso (rama classic-ciclos): cada hoja del mapa docs/casos_de_uso.md tiene un ID
// (CU-FE-..., CU-BE-..., CU-CARTA-...) y una o varias comprobaciones. Un ID con alguna comprobacion mala es FALLA; los
// marcados como DECISION (necesitan que el usuario decida: regla ambigua, diseno, contenido) no cuentan como falla.
// Al final escribe <salida> (TSV: id, estado, detalle) para ciclo_completo.sh y el resumen a stdout.
#pragma once

#include <cstdarg>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace cu {

struct Resultado {
	std::string id, estado, detalle;   // estado: OK, FALLA o DECISION
	int comprobaciones{ 0 };
};

inline std::vector<Resultado>& todos()
{
	static std::vector<Resultado> v;
	return v;
}
inline std::map<std::string, size_t>& indice()
{
	static std::map<std::string, size_t> m;
	return m;
}

inline Resultado& de(const std::string& id)
{
	auto it = indice().find(id);
	if (it != indice().end()) return todos()[it->second];
	indice()[id] = todos().size();
	todos().push_back(Resultado{ id, "OK", "", 0 });
	return todos().back();
}

// una comprobacion del caso "id": si falla, el caso queda en FALLA (con el primer detalle)
inline bool caso(const std::string& id, bool ok, const char* fmt = "", ...) __attribute__((format(printf, 3, 4)));
inline bool caso(const std::string& id, bool ok, const char* fmt, ...)
{
	Resultado& r = de(id);
	r.comprobaciones++;
	if (ok) return true;
	char buf[600];
	va_list a;
	va_start(a, fmt);
	vsnprintf(buf, sizeof(buf), fmt, a);
	va_end(a);
	if (r.estado == "OK") {
		r.estado = "FALLA";
		r.detalle = buf;
	} else if (r.estado == "FALLA" && r.detalle.size() < 400) {
		r.detalle += " | ";
		r.detalle += buf;
	}
	fprintf(stderr, "FALLA %s: %s\n", id.c_str(), buf);
	return false;
}

// una falla que necesita una decision del usuario (docs/decisiones_pendientes.md): no cuenta como falla
inline void decision(const std::string& id, const std::string& detalle)
{
	Resultado& r = de(id);
	r.comprobaciones++;
	r.estado = "DECISION";
	r.detalle = detalle;
}

inline int resumen(const char* salida)
{
	int ok = 0, mal = 0, dec = 0;
	FILE* f = salida ? fopen(salida, "w") : nullptr;
	for (const Resultado& r : todos()) {
		(r.estado == "OK" ? ok : r.estado == "FALLA" ? mal : dec)++;
		if (f) {
			std::string d = r.detalle;
			for (char& c : d)
				if (c == '\t' || c == '\n') c = ' ';
			fprintf(f, "%s\t%s\t%s\n", r.id.c_str(), r.estado.c_str(), d.c_str());
		}
	}
	if (f) fclose(f);
	printf("CASOS DE USO: %d casos, OK %d, FALLA %d, DECISION %d\n", (int)todos().size(), ok, mal, dec);
	for (const Resultado& r : todos())
		if (r.estado == "FALLA") printf("  FALLA %s: %s\n", r.id.c_str(), r.detalle.c_str());
	return mal ? 1 : 0;
}

}  // namespace cu
