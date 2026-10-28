/*
 * Yugioh Classic Remaster - el arranque de la version de Windows (en lugar de ps5/src/main.cpp; docs/pc_jugable.md).
 *
 * El juego es el mismo programa que en la consola (ps5Samples[0], examples/yugireload/yugireload.cpp): aca solo se
 * eligen la ventana (resolucion, pantalla completa, V-sync), las carpetas (assets junto al .exe en datos/, guardado
 * en %APPDATA%\YugiohClassic o, con portable.txt, en guardado\ junto al .exe) y el registro, y se corre el bucle
 * Win32 de la base (que llama a ps5HandleInput como el de la consola: parche_base.py).
 *
 * Opciones (linea de comandos o opciones_pc.txt junto al .exe, una por linea):
 *   --ancho N --alto N   tamano de la ventana (por defecto 1920x1080, o el 90 % del escritorio si es mas chico)
 *   --completa           pantalla completa sin bordes a la resolucion del escritorio (4K en un monitor 4K)
 *   --ventana            en ventana (manda sobre --completa de opciones_pc.txt)
 *   --sin-vsync          sin V-sync (por defecto va con V-sync a la frecuencia del monitor: 60, 120, 144 Hz...)
 *   --consola            los mensajes a una consola en vez de al registro
 *   --validacion         capas de validacion de Vulkan (si estan instaladas)
 */
#include "vulkanexamplebase.h"
#include "ps5_samples.h"
#include "raton_plataforma.h"

#include <shlobj.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>


namespace {

VulkanExampleBase *juego = nullptr;

// Alt+Enter / F11: pantalla completa en caliente (sin bordes, al tamano del monitor) y de vuelta a la ventana. El cambio de
// tamano llega como WM_SIZE y la base recrea la cadena de imagenes; el juego usa width / height de cada cuadro
bool pantallaCompleta = false;
LONG_PTR estiloVentana = 0;
RECT rectVentana = { 0, 0, 0, 0 };

void alternarPantallaCompleta(HWND h)
{
	if (!pantallaCompleta) {
		estiloVentana = GetWindowLongPtr(h, GWL_STYLE);
		GetWindowRect(h, &rectVentana);
		MONITORINFO mi{};
		mi.cbSize = sizeof(mi);
		GetMonitorInfo(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
		SetWindowLongPtr(h, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
		SetWindowPos(h, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
		             SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		pantallaCompleta = true;
	} else {
		// (si arranco en pantalla completa no hay estilo de ventana guardado: el de siempre, 90 % del escritorio en 16:9)
		const LONG_PTR normal = (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX)) | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
		if (rectVentana.right <= rectVentana.left || !(estiloVentana & WS_CAPTION)) {
			estiloVentana = normal;
			const int escH = GetSystemMetrics(SM_CYSCREEN) * 9 / 10, escW = escH * 16 / 9;
			RECT r = { 0, 0, escW, escH };
			AdjustWindowRectEx(&r, (DWORD)normal, FALSE, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
			const int ancho = r.right - r.left, alto = r.bottom - r.top;
			rectVentana = { (GetSystemMetrics(SM_CXSCREEN) - ancho) / 2, (GetSystemMetrics(SM_CYSCREEN) - alto) / 2, 0, 0 };
			rectVentana.right = rectVentana.left + ancho;
			rectVentana.bottom = rectVentana.top + alto;
		}
		SetWindowLongPtr(h, GWL_STYLE, estiloVentana);
		SetWindowPos(h, HWND_NOTOPMOST, rectVentana.left, rectVentana.top, rectVentana.right - rectVentana.left, rectVentana.bottom - rectVentana.top,
		             SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		pantallaCompleta = false;
	}
}

LRESULT CALLBACK procesarVentana(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_SYSKEYDOWN:
		if (wParam == VK_RETURN && (lParam & (1 << 29))) {   // Alt+Enter
			if (!(lParam & (1 << 30))) alternarPantallaCompleta(hWnd);
			return 0;
		}
		break;
	case WM_KEYDOWN:
		if (wParam == VK_F11 && !(lParam & (1 << 30))) {
			alternarPantallaCompleta(hWnd);
			return 0;
		}
		break;
	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT) {
			SetCursor(raton_cursor_oculto() ? nullptr : LoadCursor(nullptr, IDC_ARROW));
			return TRUE;
		}
		break;
	case WM_MOUSEMOVE:
	case WM_MOUSELEAVE:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
		raton_mensaje(hWnd, uMsg, wParam, lParam);   // el raton lo lee el juego (src/raton.h), no la camara de la base
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	if (juego) {
		juego->handleMessages(hWnd, uMsg, wParam, lParam);
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// con barras normales y terminada en '/' (fopen y las rutas del juego aceptan '/')
std::string carpeta(std::string r)
{
	for (char &c : r) {
		if (c == '\\') c = '/';
	}
	if (!r.empty() && r.back() != '/') r += '/';
	return r;
}

std::string carpetaDelExe()
{
	char ruta[MAX_PATH];
	DWORD n = GetModuleFileNameA(nullptr, ruta, MAX_PATH);
	std::string r(ruta, n);
	const size_t barra = r.find_last_of("\\/");
	return carpeta(barra == std::string::npos ? "." : r.substr(0, barra));
}

bool existe(const std::string &ruta)
{
	return GetFileAttributesA(ruta.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool crearCarpeta(const std::string &ruta)
{
	CreateDirectoryA(ruta.c_str(), nullptr);
	const DWORD a = GetFileAttributesA(ruta.c_str());
	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

// donde se guarda: portable.txt junto al .exe -> guardado/ ahi; si no, %APPDATA%\YugiohClassic. "" si ninguna
std::string carpetaGuardado(const std::string &exe)
{
	if (existe(exe + "portable.txt") && crearCarpeta(exe + "guardado")) {
		return exe + "guardado/";
	}
	char appdata[MAX_PATH];
	if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata) == S_OK) {
		const std::string r = carpeta(appdata) + "YugiohClassic";
		if (crearCarpeta(r)) return r + "/";
	}
	if (crearCarpeta(exe + "guardado")) return exe + "guardado/";
	return "";
}

struct Opciones {
	uint32_t ancho{ 0 }, alto{ 0 };
	bool completa{ false }, ventana{ false }, sinVsync{ false }, consola{ false }, validacion{ false };

	void leer(const std::vector<std::string> &palabras)
	{
		for (size_t i = 0; i < palabras.size(); i++) {
			const std::string &p = palabras[i];
			auto numero = [&]() { return i + 1 < palabras.size() ? (uint32_t)atoi(palabras[++i].c_str()) : 0u; };
			if (p == "--ancho" || p == "-w") ancho = numero();
			else if (p == "--alto" || p == "-h") alto = numero();
			else if (p == "--completa" || p == "--fullscreen" || p == "-f") completa = true;
			else if (p == "--ventana") ventana = true;
			else if (p == "--sin-vsync") sinVsync = true;
			else if (p == "--consola") consola = true;
			else if (p == "--validacion") validacion = true;
		}
	}
};

void avisar(const std::string &texto)
{
	say("%s", texto.c_str());
	MessageBoxA(nullptr, texto.c_str(), "Yugioh Classic Remaster", MB_OK | MB_ICONERROR);
}

} // namespace

int main(int argc, char **argv)
{
	// los archivos se leen y escriben en binario, como en la consola (sin \r\n de por medio)
	_fmode = _O_BINARY;
	const std::string exe = carpetaDelExe();

	Opciones op;
	{
		std::ifstream archivo(exe + "opciones_pc.txt");
		std::vector<std::string> palabras;
		for (std::string w; archivo >> w;) {
			if (w[0] == '#') {
				std::string resto;
				std::getline(archivo, resto);
				continue;
			}
			palabras.push_back(w);
		}
		op.leer(palabras);
		palabras.assign(argv + 1, argv + argc);
		op.leer(palabras);
	}

	// el guardado (lo lee guardado::carpetaDatos()) y el registro (lo que en la consola va a klog)
	const std::string guardadoDir = carpetaGuardado(exe);
	if (!guardadoDir.empty()) {
		SetEnvironmentVariableA("YUGI_DATOS_PC", guardadoDir.c_str());
		_putenv_s("YUGI_DATOS_PC", guardadoDir.c_str());
	}
	if (op.consola) {
		AllocConsole();
		freopen("CONOUT$", "w", stderr);
		freopen("CONOUT$", "w", stdout);
	} else {
		const std::string registro = (guardadoDir.empty() ? exe : guardadoDir) + "yugioh_classic.log";
		if (!freopen(registro.c_str(), "w", stderr)) {
			freopen("NUL", "w", stderr);
		}
		setvbuf(stderr, nullptr, _IONBF, 0);   // (en Windows _IOLBF es buffer completo: si el juego se cae, se pierde)
		freopen("NUL", "w", stdout);
	}
	platform_init(PS5_TITLE_NAME);
	std::cout.rdbuf(std::cerr.rdbuf());
	say("juego: %s", exe.c_str());
	say("guardado: %s", guardadoDir.empty() ? "(sin carpeta)" : guardadoDir.c_str());

	// los assets y shaders del titulo: datos/assets y datos/shaders junto al .exe
	vks::tools::resourcePath = exe + "datos";
	if (!existe(exe + "datos/assets/ui")) {
		avisar("Falta la carpeta datos\\assets junto al juego (" + exe + "datos). Arma la carpeta con armar_pc.sh.");
		return 1;
	}
	if (volkInitialize() != VK_SUCCESS) {
		avisar("No se encontro Vulkan (vulkan-1.dll). Instala o actualiza el driver de la tarjeta grafica.");
		return 1;
	}
	pad_open();

	int estado = 0;
	try {
		juego = ps5Samples[0].create();
		juego->ps5.optionsEnds = false;   // un solo programa: OPTIONS no lo cierra (Ajustes: OPTIONS 2 s)
		juego->settings.validation = op.validacion;
		juego->settings.vsync = !op.sinVsync;
		const uint32_t escW = (uint32_t)GetSystemMetrics(SM_CXSCREEN), escH = (uint32_t)GetSystemMetrics(SM_CYSCREEN);
		if (op.completa && !op.ventana) {
			pantallaCompleta = true;
			juego->settings.fullscreen = true;
			juego->width = escW;
			juego->height = escH;
		} else {
			juego->settings.fullscreen = false;
			uint32_t w = op.ancho ? op.ancho : 1920, h = op.alto ? op.alto : 1080;
			if (!op.ancho && !op.alto && (w > escW * 9 / 10 || h > escH * 9 / 10)) {
				// escritorio chico: el 90 % de su alto, en 16:9
				h = escH * 9 / 10;
				w = h * 16 / 9;
			}
			juego->width = w;
			juego->height = h;
		}
		// la ventana de Ajustes (desarrollo) se escalaba para la tele 4K
		juego->ui.scale = std::max(1.0f, 2.5f * (float)juego->height / 2160.0f);
		say("ventana: %ux%u%s, V-sync %s", juego->width, juego->height, juego->settings.fullscreen ? " (pantalla completa)" : "",
			juego->settings.vsync ? "si" : "no");
		if (!juego->initVulkan()) {
			throw std::runtime_error("initVulkan fallo");
		}
		if (!juego->setupWindow(GetModuleHandle(nullptr), procesarVentana)) {
			throw std::runtime_error("no se pudo crear la ventana");
		}
		juego->prepare();
		juego->renderLoop();
		if (juego->vulkanDevice) {
			vkDeviceWaitIdle(juego->vulkanDevice->logicalDevice);
		}
	} catch (const std::exception &e) {
		avisar(std::string("El juego se cerro por un error:\n") + e.what());
		estado = 1;
	}
	VulkanExampleBase *j = juego;
	juego = nullptr;
	delete j;
	say("termina: estado %d", estado);
	return estado;
}
