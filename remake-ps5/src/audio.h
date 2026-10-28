/*
 * Yugioh Classic Remaster - el sonido (rama classic-audio).
 *
 * Un mezclador propio sobre la salida de la plantilla (platform.h: audio_start, 48 kHz, 16 bits, estereo, bloques de
 * 256 cuadros; en la PS5 es sceAudioOutOpen/sceAudioOutOutput en su propio hilo; en el PC de prueba no hay salida y
 * todo esto queda mudo sin errores). El hilo de la plataforma llama a llenar() cada ~5 ms: suma hasta 16 efectos y la
 * musica (dos voces para el fundido cruzado) y nunca espera al render: el render solo encola pedidos bajo un candado
 * de microsegundos.
 *
 * Formatos (assets/audio, no van al repo: los saca herramientas/audio de la copia del usuario):
 *   - efectos: WAV PCM 16 bits 48 kHz (estereo o mono). Se cargan enteros en memoria y se mezclan sin decodificar
 *     ni remuestrear: lo mas barato para sonidos cortos que se repiten.
 *   - musica: OGG Vorbis (stb_vorbis, dominio publico). Se guarda comprimido en memoria (~2-4 MB por pista) y se
 *     decodifica en el hilo de audio a medida que suena: una pista entera en PCM serian ~35 MB.
 * audio.txt: una linea por sonido "clave|archivo|volumen|bucle" (# comentario). La clave es lo que pide el juego
 * (sfx_invocar, musica_duelo...); cambiar que archivo suena es solo editar esa linea. bucle: 0 una vez, 1 en bucle;
 * la musica puede decir "1:<inicio>:<fin>" en muestras: al llegar a <fin> vuelve a <inicio> (los puntos de bucle de
 * la pista original: la intro suena una sola vez y la cola final no se oye).
 */
#pragma once

#include "clips_md.h"   // rama classic-md-fases: los tiempos de DuelLP de Master Duel
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// stb_vorbis entero aca: el juego es una sola unidad de compilacion (yugireload.cpp)
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "stb_vorbis.c"
// sus macros cortas (L, C, R, CHECK...) no siguen al resto del juego
#undef ADDEND
#undef C
#undef CHECK
#undef CODEBOOK_ELEMENT
#undef CODEBOOK_ELEMENT_BASE
#undef CODEBOOK_ELEMENT_FAST
#undef CRC32_POLY
#undef DECODE
#undef DECODE_RAW
#undef DECODE_VQ
#undef DIVTAB_DENOM
#undef DIVTAB_NUMER
#undef EOP
#undef FASTDEF
#undef FAST_HUFFMAN_TABLE_MASK
#undef FAST_HUFFMAN_TABLE_SIZE
#undef FAST_SCALED_FLOAT_TO_INT
#undef INVALID_BITS
#undef IS_PUSH_MODE
#undef L
#undef LIBVORBIS_MDCT
#undef LINE_OP
#undef MAGIC
#undef MAX_BLOCKSIZE
#undef MAX_BLOCKSIZE_LOG
#undef NO_CODE
#undef PAGEFLAG_continued_packet
#undef PAGEFLAG_first_page
#undef PAGEFLAG_last_page
#undef PLAYBACK_LEFT
#undef PLAYBACK_MONO
#undef PLAYBACK_RIGHT
#undef R
#undef SAMPLE_unknown
#undef USE_MEMORY
#undef array_size_required
#undef check_endianness
#undef temp_alloc
#undef temp_alloc_restore
#undef temp_alloc_save
#undef temp_block_array
#undef temp_free

namespace audio {

constexpr int FRECUENCIA = 48000;
constexpr int VOCES_EFECTOS = 16;

struct Efecto { std::vector<int16_t> pcm; float vol{ 1.0f }; bool bucle{ false }; };   // estereo intercalado, 48 kHz
struct Pista { std::vector<uint8_t> ogg; float vol{ 1.0f }; bool bucle{ true }; unsigned inicioBucle{ 0 }, finBucle{ 0 }; };

class Motor {
public:
	// volumenes que el juego (Ajustes) cambia en cualquier momento; el hilo los lee en cada bloque
	std::atomic<float> volGeneral{ 1.0f }, volMusica{ 0.7f }, volEfectos{ 1.0f };

	~Motor() { parar(); }

	// lee <carpeta>/audio.txt y sus archivos y abre la salida. false si no hay audio.txt (el juego sigue mudo)
	bool iniciar(const std::string& carpeta)
	{
		FILE* f = fopen((carpeta + "audio.txt").c_str(), "rb");
		if (!f) {
			fprintf(stderr, "[audio] sin %saudio.txt: el juego queda mudo\n", carpeta.c_str());
			return false;
		}
		char linea[512];
		int nE = 0, nM = 0;
		while (fgets(linea, sizeof(linea), f)) {
			std::string s(linea);
			while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
			if (s.empty() || s[0] == '#') continue;
			std::vector<std::string> c;
			size_t a = 0;
			for (size_t b; (b = s.find('|', a)) != std::string::npos; a = b + 1) c.push_back(s.substr(a, b - a));
			c.push_back(s.substr(a));
			if (c.size() < 2) continue;
			const float vol = c.size() > 2 ? (float)atof(c[2].c_str()) : 1.0f;
			const bool bucle = c.size() > 3 && atoi(c[3].c_str()) != 0;
			std::vector<uint8_t> bytes;
			if (!leer(carpeta + c[1], bytes)) {
				fprintf(stderr, "[audio] falta %s (%s)\n", c[1].c_str(), c[0].c_str());
				continue;
			}
			const std::string ext = c[1].size() > 4 ? c[1].substr(c[1].size() - 4) : "";
			if (ext == ".ogg") {
				Pista& p = pistas[c[0]];
				p.ogg.swap(bytes);
				p.vol = vol;
				p.bucle = bucle;
				const size_t dos = c.size() > 3 ? c[3].find(':') : std::string::npos;
				if (dos != std::string::npos) {
					char* fin = nullptr;
					p.inicioBucle = (unsigned)strtoul(c[3].c_str() + dos + 1, &fin, 10);
					if (fin && *fin == ':') p.finBucle = (unsigned)strtoul(fin + 1, nullptr, 10);
					if (p.finBucle <= p.inicioBucle) p.finBucle = 0;
				}
				nM++;
			} else {
				Efecto e;
				if (!wav(bytes, e.pcm)) {
					fprintf(stderr, "[audio] %s: WAV no soportado (debe ser PCM 16 bits 48 kHz)\n", c[1].c_str());
					continue;
				}
				e.vol = vol;
				e.bucle = bucle;
				efectos[c[0]] = std::move(e);
				nE++;
			}
		}
		fclose(f);
		abierto = audio_start(&Motor::llenarC, this);
		fprintf(stderr, "[audio] %d efectos, %d pistas de musica; salida %s\n", nE, nM, abierto ? "abierta" : "nula (sin sonido)");
		return true;
	}

	void parar()
	{
		if (abierto) audio_stop();
		abierto = false;
		std::lock_guard<std::mutex> l(candado);
		for (Voz& v : musica) cerrar(v);
	}

	bool hay(const char* clave) const { return efectos.count(clave) || pistas.count(clave); }

	// un efecto. retraso en s (p. ej. el golpe despues del ataque), pan -1 izquierda .. 1 derecha
	void sfx(const char* clave, float vol = 1.0f, float pan = 0.0f, float retraso = 0.0f)
	{
		auto it = efectos.find(clave);
		if (it == efectos.end() || it->second.pcm.empty()) return;
		std::lock_guard<std::mutex> l(candado);
		Ef* libre = nullptr;
		for (Ef& v : voces) {
			if (!v.e) { libre = &v; break; }
			if (!libre || v.pos > libre->pos) libre = &v;   // sin libres: se corta la que mas lleva sonando
		}
		libre->e = &it->second;
		libre->pos = 0;
		libre->espera = (int)(retraso * FRECUENCIA);
		const float g = vol * it->second.vol;
		const float p = std::fmax(-1.0f, std::fmin(1.0f, pan));
		libre->gi = g * (p > 0.0f ? 1.0f - p : 1.0f);
		libre->gd = g * (p < 0.0f ? 1.0f + p : 1.0f);
	}

	// cambia la musica con un fundido cruzado de "fundido" s. La misma que ya suena no se reinicia
	void tocarMusica(const char* clave, float fundido = 1.5f)
	{
		auto it = pistas.find(clave);
		std::lock_guard<std::mutex> l(candado);
		if (actual == clave && musica[0].v) return;
		actual = it == pistas.end() ? "" : clave;
		// la que sonaba pasa a la voz 1 y se apaga; la nueva entra en la 0
		cerrar(musica[1]);
		musica[1] = musica[0];
		musica[1].destino = 0.0f;
		musica[1].paso = musica[1].ganancia / std::fmax(fundido * FRECUENCIA, 1.0f);
		musica[0] = Voz{};
		if (it == pistas.end()) return;
		int err = 0;
		musica[0].v = stb_vorbis_open_memory(it->second.ogg.data(), (int)it->second.ogg.size(), &err, nullptr);
		if (!musica[0].v) {
			fprintf(stderr, "[audio] %s: OGG ilegible (%d)\n", clave, err);
			return;
		}
		musica[0].p = &it->second;
		musica[0].ganancia = 0.0f;
		musica[0].destino = 1.0f;
		musica[0].paso = 1.0f / std::fmax(fundido * FRECUENCIA, 1.0f);
	}
	void pararMusica(float fundido = 1.5f) { tocarMusica("", fundido); }
	const std::string& musicaActual() const { return actual; }

private:
	struct Ef { const Efecto* e{ nullptr }; size_t pos{ 0 }; int espera{ 0 }; float gi{ 1 }, gd{ 1 }; };
	struct Voz { stb_vorbis* v{ nullptr }; const Pista* p{ nullptr }; float ganancia{ 0 }, destino{ 0 }, paso{ 0 }; unsigned pos{ 0 }; };

	std::map<std::string, Efecto> efectos;   // no cambian despues de iniciar(): el hilo los lee sin candado
	std::map<std::string, Pista> pistas;
	std::mutex candado;
	Ef voces[VOCES_EFECTOS];
	Voz musica[2];
	std::string actual;
	bool abierto{ false };
	std::vector<float> mezcla;
	std::vector<int16_t> trozo;

	static void cerrar(Voz& v)
	{
		if (v.v) stb_vorbis_close(v.v);
		v = Voz{};
	}

	static bool leer(const std::string& ruta, std::vector<uint8_t>& out)
	{
		FILE* f = fopen(ruta.c_str(), "rb");
		if (!f) return false;
		fseek(f, 0, SEEK_END);
		const long n = ftell(f);
		fseek(f, 0, SEEK_SET);
		out.resize(n > 0 ? (size_t)n : 0);
		const bool ok = n > 0 && fread(out.data(), 1, out.size(), f) == out.size();
		fclose(f);
		return ok;
	}

	// RIFF/WAVE PCM 16 bits, mono o estereo, 48 kHz -> estereo intercalado
	static bool wav(const std::vector<uint8_t>& b, std::vector<int16_t>& pcm)
	{
		if (b.size() < 44 || memcmp(b.data(), "RIFF", 4) != 0 || memcmp(b.data() + 8, "WAVE", 4) != 0) return false;
		int canales = 0, bits = 0, frec = 0;
		size_t i = 12;
		while (i + 8 <= b.size()) {
			uint32_t tam;
			memcpy(&tam, b.data() + i + 4, 4);
			const uint8_t* d = b.data() + i + 8;
			if (memcmp(b.data() + i, "fmt ", 4) == 0 && tam >= 16) {
				uint16_t fmt, ch, bps;
				uint32_t sr;
				memcpy(&fmt, d, 2); memcpy(&ch, d + 2, 2); memcpy(&sr, d + 4, 4); memcpy(&bps, d + 14, 2);
				if (fmt != 1 && fmt != 0xFFFE) return false;
				canales = ch; bits = bps; frec = (int)sr;
			} else if (memcmp(b.data() + i, "data", 4) == 0) {
				if (bits != 16 || frec != FRECUENCIA || (canales != 1 && canales != 2)) return false;
				const size_t n = std::min<size_t>(tam, b.size() - i - 8) / (2 * canales);
				pcm.resize(n * 2);
				for (size_t k = 0; k < n; k++) {
					int16_t l, r;
					memcpy(&l, d + k * 2 * canales, 2);
					r = l;
					if (canales == 2) memcpy(&r, d + k * 4 + 2, 2);
					pcm[2 * k] = l;
					pcm[2 * k + 1] = r;
				}
				return true;
			}
			i += 8 + tam + (tam & 1);
		}
		return false;
	}

	static void llenarC(int16_t* cuadros, int n, void* yo) { static_cast<Motor*>(yo)->llenar(cuadros, n); }

	// el hilo de audio: n cuadros estereo
	void llenar(int16_t* out, int n)
	{
		mezcla.assign((size_t)n * 2, 0.0f);
		if (trozo.size() < (size_t)n * 2) trozo.resize((size_t)n * 2);
		const float gE = volGeneral.load() * volEfectos.load(), gM = volGeneral.load() * volMusica.load();
		{
			std::lock_guard<std::mutex> l(candado);
			for (Ef& v : voces) {
				if (!v.e) continue;
				int k = 0;
				if (v.espera > 0) {
					k = std::min(v.espera, n);
					v.espera -= k;
				}
				const size_t total = v.e->pcm.size() / 2;
				for (; k < n; k++) {
					if (v.pos >= total) {
						if (!v.e->bucle) { v.e = nullptr; break; }
						v.pos = 0;
					}
					mezcla[2 * k] += v.e->pcm[2 * v.pos] * v.gi * gE;
					mezcla[2 * k + 1] += v.e->pcm[2 * v.pos + 1] * v.gd * gE;
					v.pos++;
				}
			}
			for (Voz& m : musica) {
				if (!m.v) continue;
				int hecho = 0, vueltas = 0;
				while (hecho < n) {
					int pide = n - hecho;
					const bool hayFin = m.p->bucle && m.p->finBucle;
					if (hayFin) pide = (int)std::min<unsigned>((unsigned)pide, m.p->finBucle > m.pos ? m.p->finBucle - m.pos : 0u);
					const int got = pide > 0 ? stb_vorbis_get_samples_short_interleaved(m.v, 2, trozo.data() + 2 * hecho, pide * 2) : 0;
					if (got <= 0) {
						if (!m.p->bucle || ++vueltas > 2) break;
						if (m.p->inicioBucle) stb_vorbis_seek(m.v, m.p->inicioBucle);
						else stb_vorbis_seek_start(m.v);
						m.pos = m.p->inicioBucle;
						continue;
					}
					hecho += got;
					m.pos += (unsigned)got;
				}
				const float base = gM * m.p->vol;
				for (int k = 0; k < hecho; k++) {
					if (m.ganancia < m.destino) m.ganancia = std::fmin(m.destino, m.ganancia + m.paso);
					else if (m.ganancia > m.destino) m.ganancia = std::fmax(m.destino, m.ganancia - m.paso);
					mezcla[2 * k] += trozo[2 * k] * base * m.ganancia;
					mezcla[2 * k + 1] += trozo[2 * k + 1] * base * m.ganancia;
				}
				if (hecho < n || (m.destino <= 0.0f && m.ganancia <= 0.0f)) cerrar(m);
			}
		}
		for (int k = 0; k < n * 2; k++) {
			// limitador suave: lo que pasa de ~0.8 se curva en vez de recortar en seco
			float x = mezcla[k] * (1.0f / 32768.0f);
			const float a = std::fabs(x);
			if (a > 0.8f) x = std::copysign(0.8f + 0.2f * std::tanh((a - 0.8f) / 0.2f), x);
			out[k] = (int16_t)std::lrint(x * 32767.0f);
		}
	}
};

// ------------------------------------------------------------------------------------------- el duelo
// Lo que suena con el duelo, sin tocar control.h: los eventos que ya emite (EV_*) y, para lo que no tiene evento
// (mover el cursor, abrir/cerrar menus, robar, colocar boca abajo, los LP, el golpe del ataque), comparando el
// estado de un cuadro con el anterior.
struct Duelo {
	bool listo{ false };
	control::Foco foco{};
	int opcion{ 0 }, estado{ 0 }, mano{ 0 }, cemJ{ 0 }, cemR{ 0 }, fase{ -1 }, lpJ{ -1 }, lpR{ -1 };
	bool boca[control::NUM_ZONAS_MESA]{}, llena[control::NUM_ZONAS_MESA]{};
	float tAtaque{ -1.0f };
	bool hubo{ false };   // en este cuadro hubo un evento que ya sono

	// antes de vaciar ctl.eventos
	void evento(Motor& m, const control::Evento& e)
	{
		using namespace control;
		const float pan = e.zona >= 0 && e.zona < NUM_ZONAS_MESA ? ZONAS_MESA[e.zona].x / 12.0f : 0.0f;
		switch (e.tipo) {
		case EV_INVOCAR: m.sfx("sfx_invocar", 1.0f, pan); hubo = true; break;
		case EV_VOLTEAR_MONSTRUO: m.sfx("sfx_voltear", 1.0f, pan); m.sfx("sfx_invocar", 0.7f, pan, 0.25f); hubo = true; break;
		case EV_POSICION: m.sfx("sfx_posicion", 1.0f, pan); hubo = true; break;
		case EV_AL_CEMENTERIO: m.sfx("sfx_destruir", 1.0f, pan); hubo = true; break;
		case EV_A_LA_MANO: m.sfx("sfx_carta_mano", 0.6f, pan); hubo = true; break;
		case EV_ACTIVAR_MAGIA: m.sfx("sfx_activar", 1.0f, pan); hubo = true; break;
		case EV_ACTIVAR_CAMPO: m.sfx("sfx_activar_campo", 1.0f, pan); hubo = true; break;
		case EV_DESTRUIR_CAMPO: m.sfx("sfx_destruir", 1.0f, pan); hubo = true; break;
		default: break;   // EV_ATACAR y EV_FASE: los mira cuadro() (la demo ataca sin evento)
		}
	}

	// una vez por cuadro, despues de avanzar el control. pulsos: los botones logicos del cuadro (0 si los ajustes
	// tienen el mando); tAtaque/tImpacto: el reloj del ataque en curso (<0 sin ataque)
	void cuadro(Motor& m, const control::Mando& c, uint32_t pulsos, float tAtaque_, float tImpacto, int lpJ_, int lpR_)
	{
		using namespace control;
		if (!listo) {
			guardar(c, lpJ_, lpR_);
			tAtaque = tAtaque_;
			listo = true;
			return;
		}
		bool sono = hubo;
		// el ataque: al empezar y en el golpe
		if (tAtaque_ >= 0.0f && (tAtaque < 0.0f || tAtaque_ < tAtaque)) m.sfx("sfx_atacar");
		if (tAtaque_ >= tImpacto && tAtaque >= 0.0f && tAtaque < tImpacto) m.sfx("sfx_impacto");
		tAtaque = tAtaque_;
		if (c.fase != fase) {
			char k[16];
			snprintf(k, sizeof(k), "sfx_fase_%d", c.fase);   // cada fase su sonido (interfaz::FASES); si no, el comun
			m.sfx(m.hay(k) ? k : "sfx_fase");
			if (c.fase == 0 && !c.duelo) m.sfx("sfx_turno", 0.8f, 0.0f, 0.35f);   // vuelve a la fase de robo: turno nuevo (con el duelo: el cartel de turno de MD, fases_md_juego.h)
			sono = true;
		}
		// LP: el golpe y el contador
		// rama classic-md-fases: la cuenta de LP de Master Duel (DuelLP: SE_LP_COUNT_PLAYER/RIVAL o SE_LP_RECOVERY_*) suena
		// cuando el valor que vuela llega al panel (OnEffectEnd de LPCounterSub: el inicio de Dissolve en su cadena)
		float tCuenta = 0.15f;
		if (const clipsmd::Prefab* p = clipsmd::biblioteca().prefab("DuelLPNear")) {
			const float t0 = p->inicioEnCadena("ChangeValue", "ZoomIn", "ChangeValue", "Dissolve");
			if (t0 >= 0.0f) tCuenta = t0;
		}
		auto cuenta = [&](bool rival, bool cura) {
			const char* k = cura ? (rival ? "sfx_lp_cura_rival" : "sfx_lp_cura") : (rival ? "sfx_lp_rival" : "sfx_lp_jugador");
			m.sfx(m.hay(k) ? k : "sfx_lp", 1.0f, 0.0f, tCuenta);
		};
		if (lpJ_ < lpJ || lpR_ < lpR) {
			m.sfx("sfx_danio");
			cuenta(lpR_ < lpR && !(lpJ_ < lpJ), false);
			sono = true;
		} else if (lpJ_ > lpJ || lpR_ > lpR) {
			cuenta(lpR_ > lpR && !(lpJ_ > lpJ), true);
			sono = true;
		}
		if ((int)c.manoJ.size() > mano && c.estado != VOLANDO) { m.sfx("sfx_robar"); sono = true; }
		for (int z = 0; z < NUM_ZONAS_MESA; z++) {
			const bool ll = !c.zonas[z].vacia(), bo = ll && c.zonas[z].bocaAbajo;
			if (ll && !llena[z] && bo) { m.sfx("sfx_colocar", 1.0f, ZONAS_MESA[z].x / 12.0f); sono = true; }
			else if (ll && !llena[z] && !hubo && !esTipo(z, "monstruo")) { m.sfx("sfx_colocar", 0.8f, ZONAS_MESA[z].x / 12.0f); sono = true; }
		}
		if (!hubo && ((int)c.cementerioJ.size() > cemJ || (int)c.cementerioR.size() > cemR)) { m.sfx("sfx_destruir", 0.8f); sono = true; }
		if (c.estado != estado) {
			if (c.estado == VOLANDO) m.sfx("sfx_carta_mover");
			else if (c.estado == DETALLE) m.sfx(m.hay("sfx_detalle") ? "sfx_detalle" : "sfx_confirmar");
			else if (c.estado == MENU || c.estado == ELEGIR_ZONA || c.estado == ELEGIR_OBJETIVO) {
				if (estado == NAVEGAR || estado == MENU) m.sfx("sfx_confirmar");
			} else if ((pulsos & B_CIRCULO) && !sono) m.sfx("sfx_cancelar");
			sono = true;
		}
		if (!sono) {
			if (!(c.foco == foco) || c.opcion != opcion) m.sfx("sfx_cursor", 0.7f);
			else if (pulsos & B_CIRCULO) m.sfx("sfx_cancelar", 0.6f);
			else if (pulsos & B_X) m.sfx("sfx_error", 0.6f);   // X sin efecto (opcion inactiva)
		}
		guardar(c, lpJ_, lpR_);
		hubo = false;
	}

private:
	void guardar(const control::Mando& c, int lj, int lr)
	{
		foco = c.foco;
		opcion = c.opcion;
		estado = (int)c.estado;
		mano = (int)c.manoJ.size();
		cemJ = (int)c.cementerioJ.size();
		cemR = (int)c.cementerioR.size();
		fase = c.fase;
		lpJ = lj;
		lpR = lr;
		for (int z = 0; z < control::NUM_ZONAS_MESA; z++) {
			llena[z] = !c.zonas[z].vacia();
			boca[z] = llena[z] && c.zonas[z].bocaAbajo;
		}
	}
};

}   // namespace audio
