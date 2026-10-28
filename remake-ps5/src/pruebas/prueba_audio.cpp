// Prueba del mezclador sin consola (rama classic-audio): carga assets/audio, toca la musica y unos efectos y
// escribe 12 s de la mezcla a un WAV. Uso (en arch-ps5):
//   clang++ -std=c++17 -O1 -I src -I third_party src/pruebas/prueba_audio.cpp -o /tmp/prueba_audio && /tmp/prueba_audio <carpeta audio/> /tmp/mezcla.wav [segundos] [musica]
#include <cstdint>
#include <cstdio>
static void (*g_fill)(int16_t*, int, void*) = nullptr;
static void* g_user = nullptr;
typedef void (*audio_fill_fn)(int16_t* frames, int count, void* user);
static bool audio_start(audio_fill_fn f, void* u) { g_fill = f; g_user = u; return true; }
static void audio_stop() {}
#include "control.h"
#include "audio.h"
int main(int argc, char** argv)
{
	audio::Motor m;
	if (argc < 3 || !m.iniciar(argv[1])) return 1;
	const int seg = argc > 3 ? atoi(argv[3]) : 12;
	m.tocarMusica(argc > 4 ? argv[4] : "musica_duelo", 1.0f);
	std::vector<int16_t> out;
	int16_t g[256 * 2];
	double pico = 0;
	for (int b = 0; b < seg * 48000 / 256; b++) {
		const float t = b * 256 / 48000.0f;
		if (b == 300) m.sfx("sfx_invocar", 1.0f, -0.5f);
		if (b == 600) { m.sfx("sfx_atacar"); m.sfx("sfx_impacto", 1.0f, 0.0f, 0.5f); }
		if (b == 1000 && argc <= 4) m.tocarMusica("musica_duelo_tensa", 2.0f);
		if (b == 1500) { for (int k = 0; k < 20; k++) m.sfx("sfx_cursor"); }   // mas voces que las 16
		g_fill(g, 256, g_user);
		for (int k = 0; k < 512; k++) { out.push_back(g[k]); pico = std::max(pico, (double)std::abs((int)g[k])); }
		(void)t;
	}
	FILE* f = fopen(argv[2], "wb");
	uint32_t n = out.size() * 2, v;
	fwrite("RIFF", 1, 4, f); v = 36 + n; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
	uint32_t c16 = 16; uint16_t pcm = 1, ch = 2, bits = 16, al = 4; uint32_t sr = 48000, br = 48000 * 4;
	fwrite(&c16, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&al, 2, 1, f); fwrite(&bits, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); fwrite(out.data(), 2, out.size(), f); fclose(f);
	printf("pico %.0f, musica %s\n", pico, m.musicaActual().c_str());
	return pico > 1000 ? 0 : 2;
}
