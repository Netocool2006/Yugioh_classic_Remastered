/*
 * Yugioh Classic Remaster - platform.h en Windows (la version jugable de PC; docs/pc_jugable.md).
 *
 * Lo mismo que ps5/src/platform.c hace en la consola: mensajes, hora, mando y sonido, para que el juego no cambie:
 *  - say: a stderr (main_windows.cpp lo manda al registro yugioh_classic.log)
 *  - mando: XInput (Xbox) cargado en tiempo de ejecucion; si SDL3.dll esta junto al .exe, SDL3 (licencia zlib) para
 *    DualSense, DualShock 4, Switch y tambien Xbox (sin leerlos dos veces). Sin cabeceras ni .lib de SDL: las pocas
 *    funciones que se usan se piden con GetProcAddress (su ABI es estable en SDL3). El teclado es el respaldo, solo
 *    con la ventana del juego al frente. Todo se junta en el jugador 0 con los mismos bits PAD_* de la consola.
 *  - sonido: waveOut (winmm) con un hilo propio que pide bloques de 256 cuadros a fill, 48 kHz estereo 16 bits.
 *  - pruebas sin manos: YUGI_GUION_PC="8:X 11:X 14:ABAJO ..." aprieta esos botones (0,15 s) a esos segundos desde el
 *    primer cuadro (la primera lectura del mando, ya cargado el juego), sin teclado ni mando de verdad (no toca el escritorio de nadie)
 */
#include "platform.h"
#include "raton_plataforma.h"

#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <xinput.h>

#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *say_prefix = "";

/* ------------------------------------------------------------------------------------------------ mensajes y hora */

void
platform_init(const char *title_name)
{
   static char prefix[64];
   snprintf(prefix, sizeof(prefix), "[%s] ", title_name);
   say_prefix = prefix;
   say("arranca (Windows)");
}

void
say(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   fputs(say_prefix, stderr);
   vfprintf(stderr, format, args);
   va_end(args);
   fputc('\n', stderr);
   fflush(stderr);
}

double
now_seconds(void)
{
   static LARGE_INTEGER frecuencia;
   LARGE_INTEGER t;
   if (!frecuencia.QuadPart) {
      QueryPerformanceFrequency(&frecuencia);
   }
   QueryPerformanceCounter(&t);
   return (double)t.QuadPart / (double)frecuencia.QuadPart;
}

/* ----------------------------------------------------------------------------------------------------- el mando */

/* zona muerta de los sticks (0..1) y umbral del bit digital de los gatillos */
#define ZONA_MUERTA 0.24f
#define GATILLO_DIGITAL 0.35f
#define PAD_L2_DIGITAL 0x000100u
#define PAD_R2_DIGITAL 0x000200u

/* lo que leyo el ultimo pad_poll (jugador 0: todos los mandos y el teclado juntos) */
static struct pad estado_actual;
static bool hay_mando;      /* algun mando conectado en la ultima lectura */
static double ultimo_aviso; /* para no llenar el registro */

static float
zona_muerta(float v)
{
   const float a = fabsf(v);
   if (a < ZONA_MUERTA) {
      return 0.0f;
   }
   const float r = (a - ZONA_MUERTA) / (1.0f - ZONA_MUERTA);
   return v < 0.0f ? -fminf(r, 1.0f) : fminf(r, 1.0f);
}

/* el mas fuerte de dos ejes (dos mandos o mando y teclado a la vez) */
static float
mas_fuerte(float a, float b)
{
   return fabsf(b) > fabsf(a) ? b : a;
}

/* --- XInput */
typedef DWORD(WINAPI *xinput_get_state_fn)(DWORD, XINPUT_STATE *);
typedef DWORD(WINAPI *xinput_set_state_fn)(DWORD, XINPUT_VIBRATION *);
static xinput_get_state_fn xinput_get_state;
static xinput_set_state_fn xinput_set_state;
static bool xinput_conectado[XUSER_MAX_COUNT];
static double xinput_revisar[XUSER_MAX_COUNT]; /* un puerto vacio se revisa una vez por segundo (es lento) */

static void
xinput_abrir(void)
{
   static const char *const dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
   for (size_t i = 0; i < sizeof(dlls) / sizeof(dlls[0]) && !xinput_get_state; i++) {
      HMODULE m = LoadLibraryA(dlls[i]);
      if (m) {
         xinput_get_state = (xinput_get_state_fn)(void (*)(void))GetProcAddress(m, "XInputGetState");
         xinput_set_state = (xinput_set_state_fn)(void (*)(void))GetProcAddress(m, "XInputSetState");
         if (xinput_get_state) {
            say("mando: XInput (%s)", dlls[i]);
         }
      }
   }
}

static void
xinput_leer(struct pad *p)
{
   if (!xinput_get_state) {
      return;
   }
   const double ahora = now_seconds();
   for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
      if (!xinput_conectado[i] && ahora < xinput_revisar[i]) {
         continue;
      }
      XINPUT_STATE s;
      memset(&s, 0, sizeof(s));
      if (xinput_get_state(i, &s) != ERROR_SUCCESS) {
         if (xinput_conectado[i]) {
            say("mando: XInput %lu desconectado", (unsigned long)i);
         }
         xinput_conectado[i] = false;
         xinput_revisar[i] = ahora + 1.0;
         continue;
      }
      if (!xinput_conectado[i]) {
         say("mando: XInput %lu conectado", (unsigned long)i);
      }
      xinput_conectado[i] = true;
      hay_mando = true;
      const WORD b = s.Gamepad.wButtons;
      static const struct { WORD x; uint32_t pad; } tabla[] = {
         { XINPUT_GAMEPAD_A, PAD_CROSS }, { XINPUT_GAMEPAD_B, PAD_CIRCLE }, { XINPUT_GAMEPAD_X, PAD_SQUARE },
         { XINPUT_GAMEPAD_Y, PAD_TRIANGLE }, { XINPUT_GAMEPAD_LEFT_SHOULDER, PAD_L1 },
         { XINPUT_GAMEPAD_RIGHT_SHOULDER, PAD_R1 }, { XINPUT_GAMEPAD_LEFT_THUMB, PAD_L3 },
         { XINPUT_GAMEPAD_RIGHT_THUMB, PAD_R3 }, { XINPUT_GAMEPAD_START, PAD_OPTIONS },
         { XINPUT_GAMEPAD_BACK, PAD_TOUCH_PAD }, { XINPUT_GAMEPAD_DPAD_UP, PAD_UP },
         { XINPUT_GAMEPAD_DPAD_DOWN, PAD_DOWN }, { XINPUT_GAMEPAD_DPAD_LEFT, PAD_LEFT },
         { XINPUT_GAMEPAD_DPAD_RIGHT, PAD_RIGHT },
      };
      for (size_t k = 0; k < sizeof(tabla) / sizeof(tabla[0]); k++) {
         if (b & tabla[k].x) {
            p->held |= tabla[k].pad;
         }
      }
      /* XInput: y hacia arriba es positivo; en la consola y es positivo hacia abajo */
      p->left_x = mas_fuerte(p->left_x, zona_muerta(s.Gamepad.sThumbLX / 32767.0f));
      p->left_y = mas_fuerte(p->left_y, zona_muerta(-s.Gamepad.sThumbLY / 32767.0f));
      p->right_x = mas_fuerte(p->right_x, zona_muerta(s.Gamepad.sThumbRX / 32767.0f));
      p->right_y = mas_fuerte(p->right_y, zona_muerta(-s.Gamepad.sThumbRY / 32767.0f));
      p->l2 = fmaxf(p->l2, s.Gamepad.bLeftTrigger / 255.0f);
      p->r2 = fmaxf(p->r2, s.Gamepad.bRightTrigger / 255.0f);
   }
}

/* --- SDL3 (opcional: SDL3.dll junto al .exe) */
typedef struct SDL_Gamepad SDL_Gamepad;
typedef uint32_t SDL_JoystickID;
#define SDL_INIT_GAMEPAD 0x00002000u
static struct {
   bool listo;
   bool (*init)(uint32_t);
   bool (*set_hint)(const char *, const char *);
   void (*pump_events)(void);
   void (*flush_events)(uint32_t, uint32_t);
   SDL_JoystickID *(*get_gamepads)(int *);
   void (*free)(void *);
   SDL_Gamepad *(*open_gamepad)(SDL_JoystickID);
   bool (*gamepad_connected)(SDL_Gamepad *);
   void (*close_gamepad)(SDL_Gamepad *);
   const char *(*gamepad_name)(SDL_Gamepad *);
   bool (*get_button)(SDL_Gamepad *, int);
   int16_t (*get_axis)(SDL_Gamepad *, int);
   bool (*rumble)(SDL_Gamepad *, uint16_t, uint16_t, uint32_t);
   bool (*set_led)(SDL_Gamepad *, uint8_t, uint8_t, uint8_t);
} sdl;
#define SDL_MANDOS 4
static SDL_Gamepad *sdl_mandos[SDL_MANDOS];
static bool sdl_es_ps[SDL_MANDOS];   /* DualSense / DualShock 4 (por el nombre): los iconos de la ayuda son los de PlayStation */
static bool sdl_ultimo_ps;           /* el ultimo mando con actividad es de PlayStation */
static double sdl_revisar;

/* botones de SDL3 (SDL_GamepadButton) -> bits de la consola */
static const uint32_t sdl_botones[21] = {
   PAD_CROSS, PAD_CIRCLE, PAD_SQUARE, PAD_TRIANGLE, /* SOUTH, EAST, WEST, NORTH */
   PAD_TOUCH_PAD, 0, PAD_OPTIONS,                  /* BACK, GUIDE, START */
   PAD_L3, PAD_R3, PAD_L1, PAD_R1,                 /* LEFT_STICK, RIGHT_STICK, LEFT_SHOULDER, RIGHT_SHOULDER */
   PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT,          /* DPAD */
   0, 0, 0, 0, 0,                                  /* MISC1, paletas */
   PAD_TOUCH_PAD,                                  /* TOUCHPAD (el panel del DualSense) */
};

static void
sdl_abrir(void)
{
   /* solo la DLL de la carpeta del juego (no una cualquiera del PATH) */
   char ruta[MAX_PATH];
   DWORD n = GetModuleFileNameA(NULL, ruta, MAX_PATH);
   while (n > 0 && ruta[n - 1] != '\\' && ruta[n - 1] != '/') {
      n--;
   }
   ruta[n] = 0;
   strncat(ruta, "SDL3.dll", MAX_PATH - strlen(ruta) - 1);
   HMODULE m = LoadLibraryA(ruta);
   if (!m) {
      return;
   }
#define SDL_FN(campo, nombre) *(FARPROC *)&sdl.campo = GetProcAddress(m, nombre)
   SDL_FN(init, "SDL_Init");
   SDL_FN(set_hint, "SDL_SetHint");
   SDL_FN(pump_events, "SDL_PumpEvents");
   SDL_FN(flush_events, "SDL_FlushEvents");
   SDL_FN(get_gamepads, "SDL_GetGamepads");
   SDL_FN(free, "SDL_free");
   SDL_FN(open_gamepad, "SDL_OpenGamepad");
   SDL_FN(gamepad_connected, "SDL_GamepadConnected");
   SDL_FN(close_gamepad, "SDL_CloseGamepad");
   SDL_FN(gamepad_name, "SDL_GetGamepadName");
   SDL_FN(get_button, "SDL_GetGamepadButton");
   SDL_FN(get_axis, "SDL_GetGamepadAxis");
   SDL_FN(rumble, "SDL_RumbleGamepad");
   SDL_FN(set_led, "SDL_SetGamepadLED");
#undef SDL_FN
   if (!sdl.init || !sdl.pump_events || !sdl.flush_events || !sdl.get_gamepads || !sdl.free || !sdl.open_gamepad ||
       !sdl.gamepad_connected || !sdl.close_gamepad || !sdl.get_button || !sdl.get_axis) {
      say("mando: SDL3.dll sin las funciones esperadas: solo XInput");
      return;
   }
   if (sdl.set_hint) {
      /* el mando se lee aunque la ventana pierda el foco un instante (el juego decide que hacer) */
      sdl.set_hint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1");
   }
   if (!sdl.init(SDL_INIT_GAMEPAD)) {
      say("mando: SDL3 no arranco: solo XInput");
      return;
   }
   sdl.listo = true;
   say("mando: SDL3 (DualSense, DualShock 4, Xbox, Switch)");
}

static void
sdl_leer(struct pad *p)
{
   sdl.pump_events();
   sdl.flush_events(0, 0xFFFFFFFFu); /* sin video nadie lee la cola: se vacia */
   const double ahora = now_seconds();
   if (ahora >= sdl_revisar) {
      /* mandos nuevos (y los que se fueron) una vez por segundo */
      sdl_revisar = ahora + 1.0;
      for (int i = 0; i < SDL_MANDOS; i++) {
         if (sdl_mandos[i] && !sdl.gamepad_connected(sdl_mandos[i])) {
            say("mando: %d desconectado", i);
            sdl.close_gamepad(sdl_mandos[i]);
            sdl_mandos[i] = NULL;
         }
      }
      int cuantos = 0;
      SDL_JoystickID *ids = sdl.get_gamepads(&cuantos);
      for (int k = 0; ids && k < cuantos; k++) {
         bool abierto = false;
         SDL_Gamepad *g = sdl.open_gamepad(ids[k]); /* (abrir dos veces devuelve el mismo) */
         for (int i = 0; i < SDL_MANDOS; i++) {
            abierto = abierto || sdl_mandos[i] == g;
         }
         for (int i = 0; i < SDL_MANDOS && !abierto && g; i++) {
            if (!sdl_mandos[i]) {
               sdl_mandos[i] = g;
               abierto = true;
               const char *nombre = sdl.gamepad_name ? sdl.gamepad_name(g) : NULL;
               sdl_es_ps[i] = nombre && (strstr(nombre, "DualSense") || strstr(nombre, "DualShock") || strstr(nombre, "PS5") ||
                                         strstr(nombre, "PS4") || strstr(nombre, "PlayStation") || strstr(nombre, "Wireless Controller") ||
                                         strstr(nombre, "Sony"));
               say("mando: %d conectado (%s)", i, nombre ? nombre : "?");
            }
         }
      }
      if (ids) {
         sdl.free(ids);
      }
   }
   for (int i = 0; i < SDL_MANDOS; i++) {
      SDL_Gamepad *g = sdl_mandos[i];
      if (!g) {
         continue;
      }
      hay_mando = true;
      const uint32_t antes_held = p->held;
      for (int b = 0; b < 21; b++) {
         if (sdl_botones[b] && sdl.get_button(g, b)) {
            p->held |= sdl_botones[b];
         }
      }
      if (p->held != antes_held || fabsf(sdl.get_axis(g, 0)) > 14000 || fabsf(sdl.get_axis(g, 1)) > 14000) {
         sdl_ultimo_ps = sdl_es_ps[i];
      }
      /* SDL: y positivo hacia abajo, como la consola; gatillos 0..32767 */
      p->left_x = mas_fuerte(p->left_x, zona_muerta(sdl.get_axis(g, 0) / 32767.0f));
      p->left_y = mas_fuerte(p->left_y, zona_muerta(sdl.get_axis(g, 1) / 32767.0f));
      p->right_x = mas_fuerte(p->right_x, zona_muerta(sdl.get_axis(g, 2) / 32767.0f));
      p->right_y = mas_fuerte(p->right_y, zona_muerta(sdl.get_axis(g, 3) / 32767.0f));
      p->l2 = fmaxf(p->l2, sdl.get_axis(g, 4) / 32767.0f);
      p->r2 = fmaxf(p->r2, sdl.get_axis(g, 5) / 32767.0f);
   }
}

/* --- teclado (respaldo): solo con la ventana del juego al frente */
static bool
ventana_al_frente(void)
{
   HWND w = GetForegroundWindow();
   DWORD proceso = 0;
   if (!w) {
      return false;
   }
   GetWindowThreadProcessId(w, &proceso);
   return proceso == GetCurrentProcessId();
}

static void
teclado_leer(struct pad *p)
{
   if (!ventana_al_frente()) {
      return;
   }
#define TECLA(vk) ((GetAsyncKeyState(vk) & 0x8000) != 0)
   static const struct { int vk; uint32_t pad; } tabla[] = {
      { VK_UP, PAD_UP }, { VK_DOWN, PAD_DOWN }, { VK_LEFT, PAD_LEFT }, { VK_RIGHT, PAD_RIGHT },
      { VK_RETURN, PAD_CROSS }, { VK_SPACE, PAD_CROSS }, { VK_ESCAPE, PAD_CIRCLE }, { VK_BACK, PAD_CIRCLE },
      { 'T', PAD_TRIANGLE }, { 'F', PAD_SQUARE }, { 'Q', PAD_L1 }, { 'E', PAD_R1 }, { 'Z', PAD_L3 }, { 'C', PAD_R3 },
      { VK_TAB, PAD_OPTIONS }, { 'O', PAD_OPTIONS }, { 'P', PAD_TOUCH_PAD },
   };
   for (size_t k = 0; k < sizeof(tabla) / sizeof(tabla[0]); k++) {
      if (TECLA(tabla[k].vk)) {
         p->held |= tabla[k].pad;
      }
   }
   if (TECLA('1')) {
      p->l2 = 1.0f;
   }
   if (TECLA('3')) {
      p->r2 = 1.0f;
   }
   const float lx = (TECLA('D') ? 1.0f : 0.0f) - (TECLA('A') ? 1.0f : 0.0f);
   const float ly = (TECLA('S') ? 1.0f : 0.0f) - (TECLA('W') ? 1.0f : 0.0f);
   const float rx = (TECLA('L') ? 1.0f : 0.0f) - (TECLA('J') ? 1.0f : 0.0f);
   const float ry = (TECLA('K') ? 1.0f : 0.0f) - (TECLA('I') ? 1.0f : 0.0f);
   p->left_x = mas_fuerte(p->left_x, lx);
   p->left_y = mas_fuerte(p->left_y, ly);
   p->right_x = mas_fuerte(p->right_x, rx);
   p->right_y = mas_fuerte(p->right_y, ry);
#undef TECLA
}

/* --- guion de pruebas (YUGI_GUION_PC) */
#define GUION_MAX 256
static struct { double t; uint32_t boton; } guion[GUION_MAX];
static int guion_n;
static double guion_desde;

static void
guion_leer(void)
{
   const char *g = getenv("YUGI_GUION_PC");
   static const struct { const char *nombre; uint32_t pad; } nombres[] = {
      { "X", PAD_CROSS }, { "O", PAD_CIRCLE }, { "T", PAD_TRIANGLE }, { "C", PAD_SQUARE }, { "ARRIBA", PAD_UP },
      { "ABAJO", PAD_DOWN }, { "IZQ", PAD_LEFT }, { "DER", PAD_RIGHT }, { "L1", PAD_L1 }, { "R1", PAD_R1 },
      { "L3", PAD_L3 }, { "R3", PAD_R3 }, { "OPTIONS", PAD_OPTIONS }, { "TOUCH", PAD_TOUCH_PAD },
   };
   while (g && *g && guion_n < GUION_MAX) {
      double t = 0.0;
      char nombre[16] = { 0 };
      int usados = 0;
      if (sscanf(g, " %lf:%15[A-Z0-9]%n", &t, nombre, &usados) != 2) {
         break;
      }
      g += usados;
      for (size_t k = 0; k < sizeof(nombres) / sizeof(nombres[0]); k++) {
         if (strcmp(nombre, nombres[k].nombre) == 0) {
            guion[guion_n].t = t;
            guion[guion_n].boton = nombres[k].pad;
            guion_n++;
         }
      }
   }
   if (guion_n) {
      say("mando: guion de prueba con %d botones", guion_n);
   }
}

static void
guion_leer_botones(struct pad *p)
{
   if (guion_desde <= 0.0) {
      guion_desde = now_seconds();
   }
   const double t = now_seconds() - guion_desde;
   for (int i = 0; i < guion_n; i++) {
      if (t >= guion[i].t && t < guion[i].t + 0.15) {
         p->held |= guion[i].boton;
      }
   }
}

bool
pad_open(void)
{
   guion_leer();
   sdl_abrir();
   if (!sdl.listo) {
      xinput_abrir();
   }
   /* el teclado siempre esta: el jugador 0 existe aunque no haya mando */
   return true;
}

/* el ultimo dispositivo con el que se jugo (iconos de la ayuda) y el raton (clases mas abajo) */
static int dispositivo = DISPOSITIVO_TECLADO;
static double t_otro_entrada; /* ultima vez que se uso mando o teclado */

static bool
hay_actividad(const struct pad *p)
{
   return p->held != 0 || fabsf(p->left_x) > 0.5f || fabsf(p->left_y) > 0.5f || fabsf(p->right_x) > 0.5f ||
          fabsf(p->right_y) > 0.5f || p->l2 > GATILLO_DIGITAL || p->r2 > GATILLO_DIGITAL;
}

static void
leer_todo(void)
{
   struct pad p, tk;
   memset(&p, 0, sizeof(p));
   memset(&tk, 0, sizeof(tk));
   hay_mando = false;
   if (sdl.listo) {
      sdl_leer(&p);
   } else {
      xinput_leer(&p);
   }
   teclado_leer(&tk);
   /* el dispositivo de la ayuda: el que se toco ultimo (el mando manda si los dos a la vez) */
   if (hay_actividad(&p)) {
      dispositivo = (sdl.listo && sdl_ultimo_ps) ? DISPOSITIVO_PLAYSTATION : DISPOSITIVO_XBOX;
      t_otro_entrada = now_seconds();
   } else if (hay_actividad(&tk)) {
      dispositivo = DISPOSITIVO_TECLADO;
      t_otro_entrada = now_seconds();
   }
   p.held |= tk.held;
   p.left_x = mas_fuerte(p.left_x, tk.left_x);
   p.left_y = mas_fuerte(p.left_y, tk.left_y);
   p.right_x = mas_fuerte(p.right_x, tk.right_x);
   p.right_y = mas_fuerte(p.right_y, tk.right_y);
   p.l2 = fmaxf(p.l2, tk.l2);
   p.r2 = fmaxf(p.r2, tk.r2);
   guion_leer_botones(&p);
   /* los gatillos tambien como botones (L2/R2 digitales de la consola) */
   if (p.l2 >= GATILLO_DIGITAL) {
      p.held |= PAD_L2_DIGITAL;
   }
   if (p.r2 >= GATILLO_DIGITAL) {
      p.held |= PAD_R2_DIGITAL;
   }
   const uint32_t antes = estado_actual.held;
   estado_actual = p;
   estado_actual.pressed = p.held & ~antes;
   if (!hay_mando && now_seconds() - ultimo_aviso > 600.0) {
      ultimo_aviso = now_seconds();
      say("mando: ninguno conectado (teclado: flechas, Enter = X, Esc = circulo, Tab = OPTIONS)");
   }
}

/* ---------------------------------------------------------------------------------------------------- el raton */

static struct {
   HWND hwnd;
   float x, y;
   int movido, izq, der, centro, rueda, resto_rueda, rastreando;
   double t_mov;
   int oculto;
} rat = { NULL, -1e4f, -1e4f, 0, 0, 0, 0, 0, 0, 0, 0.0, 0 };

int
raton_mensaje(void *hwnd, unsigned msg, unsigned long long w, long long l)
{
   rat.hwnd = (HWND)hwnd;
   switch (msg) {
   case WM_MOUSEMOVE: {
      const float x = (float)GET_X_LPARAM((LPARAM)l), y = (float)GET_Y_LPARAM((LPARAM)l);
      if (!rat.rastreando) {
         TRACKMOUSEEVENT t = { sizeof(t), TME_LEAVE, (HWND)hwnd, 0 };
         TrackMouseEvent(&t);
         rat.rastreando = 1;
      }
      if (fabsf(x - rat.x) >= 1.0f || fabsf(y - rat.y) >= 1.0f) {
         rat.x = x;
         rat.y = y;
         rat.movido = 1;
         rat.t_mov = now_seconds();
         dispositivo = DISPOSITIVO_TECLADO;
      }
      return 1;
   }
   case WM_MOUSELEAVE:
      rat.rastreando = 0;
      rat.x = rat.y = -1e4f;
      return 1;
   case WM_LBUTTONDOWN:
   case WM_RBUTTONDOWN:
   case WM_MBUTTONDOWN:
      rat.x = (float)GET_X_LPARAM((LPARAM)l);
      rat.y = (float)GET_Y_LPARAM((LPARAM)l);
      if (msg == WM_LBUTTONDOWN) {
         rat.izq = 1;
      } else if (msg == WM_RBUTTONDOWN) {
         rat.der = 1;
      } else {
         rat.centro = 1;
      }
      rat.t_mov = now_seconds();
      dispositivo = DISPOSITIVO_TECLADO;
      return 1;
   case WM_LBUTTONUP:
   case WM_RBUTTONUP:
   case WM_MBUTTONUP:
   case WM_LBUTTONDBLCLK:
   case WM_RBUTTONDBLCLK:
   case WM_MBUTTONDBLCLK:
      return 1;
   case WM_MOUSEWHEEL:
      rat.resto_rueda += (int)(short)HIWORD(w);
      rat.rueda += rat.resto_rueda / WHEEL_DELTA;
      rat.resto_rueda %= WHEEL_DELTA;
      rat.t_mov = now_seconds();
      dispositivo = DISPOSITIVO_TECLADO;
      return 1;
   }
   return 0;
}

/* el puntero se oculta si despues de mover el raton se uso mando o teclado y pasaron 3 s */
static int
puntero_oculto(void)
{
   return t_otro_entrada > rat.t_mov && now_seconds() - rat.t_mov >= 3.0;
}

int
raton_cursor_oculto(void)
{
   return puntero_oculto();
}

void
raton_tomar(struct raton_estado *r)
{
   memset(r, 0, sizeof(*r));
   RECT c = { 0, 0, 0, 0 };
   if (rat.hwnd) {
      GetClientRect(rat.hwnd, &c);
   }
   r->x = rat.x;
   r->y = rat.y;
   r->ancho = (float)(c.right - c.left);
   r->alto = (float)(c.bottom - c.top);
   r->movido = rat.movido;
   r->izq = rat.izq;
   r->der = rat.der;
   r->centro = rat.centro;
   r->rueda = rat.rueda;
   r->dispositivo = dispositivo;
   const int oculto = puntero_oculto();
   r->visible = !oculto;
   if (oculto != rat.oculto) {
      rat.oculto = oculto;
      if (rat.x >= 0.0f) {
         SetCursor(oculto ? NULL : LoadCursor(NULL, IDC_ARROW));
      }
   }
   rat.movido = rat.izq = rat.der = rat.centro = rat.rueda = 0;
}

void
pad_poll(struct pad *pad)
{
   leer_todo();
   const uint32_t antes = pad->held;
   *pad = estado_actual;
   pad->pressed = pad->held & ~antes;
}

int
pad_readings(const struct pad_reading **readings)
{
   *readings = NULL;
   return 0;
}

uint32_t
pad_players(void)
{
   return 1u;
}

bool
pad_player(int player, struct pad *pad)
{
   if (player != 0) {
      memset(pad, 0, sizeof(*pad));
      return false;
   }
   const uint32_t antes = pad->held;
   *pad = estado_actual;
   pad->pressed = pad->held & ~antes;
   return true;
}

int
pad_player_readings(int player, const struct pad_reading **readings)
{
   (void)player;
   *readings = NULL;
   return 0;
}

void
pad_vibrate(float large, float small)
{
   const uint16_t grande = (uint16_t)(fminf(fmaxf(large, 0.0f), 1.0f) * 65535.0f);
   const uint16_t chico = (uint16_t)(fminf(fmaxf(small, 0.0f), 1.0f) * 65535.0f);
   if (sdl.listo && sdl.rumble) {
      for (int i = 0; i < SDL_MANDOS; i++) {
         if (sdl_mandos[i]) {
            /* hasta que se pida otra cosa (la consola no tiene duracion): 10 s alcanza */
            sdl.rumble(sdl_mandos[i], grande, chico, (grande || chico) ? 10000u : 0u);
         }
      }
   } else if (xinput_set_state) {
      XINPUT_VIBRATION v = { grande, chico };
      for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
         if (xinput_conectado[i]) {
            xinput_set_state(i, &v);
         }
      }
   }
}

void
pad_light_bar(uint8_t r, uint8_t g, uint8_t b)
{
   if (sdl.listo && sdl.set_led) {
      for (int i = 0; i < SDL_MANDOS; i++) {
         if (sdl_mandos[i]) {
            sdl.set_led(sdl_mandos[i], r, g, b);
         }
      }
   }
}

void
pad_player_vibrate(int player, float large, float small)
{
   if (player == 0) {
      pad_vibrate(large, small);
   }
}

void
pad_player_light_bar(int player, uint8_t r, uint8_t g, uint8_t b)
{
   if (player == 0) {
      pad_light_bar(r, g, b);
   }
}

/* ------------------------------------------------------------------------------------------------------ sonido */

#define AUDIO_HZ 48000
#define AUDIO_BLOQUE 256   /* cuadros por llamada a fill, como en la consola */
#define AUDIO_POR_BUFER 2  /* bloques por bufer de waveOut */
#define AUDIO_BUFERES 6    /* ~32 ms en cola */

static struct {
   HWAVEOUT salida;
   HANDLE evento;
   HANDLE hilo;
   volatile LONG seguir;
   audio_fill_fn fill;
   void *user;
   WAVEHDR cabeceras[AUDIO_BUFERES];
   int16_t muestras[AUDIO_BUFERES][AUDIO_BLOQUE * AUDIO_POR_BUFER * 2];
} audio;

static DWORD WINAPI
audio_hilo(LPVOID nada)
{
   (void)nada;
   SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
   while (InterlockedCompareExchange(&audio.seguir, 1, 1)) {
      for (int i = 0; i < AUDIO_BUFERES; i++) {
         WAVEHDR *h = &audio.cabeceras[i];
         if (h->dwFlags & WHDR_INQUEUE) {
            continue;
         }
         for (int k = 0; k < AUDIO_POR_BUFER; k++) {
            audio.fill(audio.muestras[i] + k * AUDIO_BLOQUE * 2, AUDIO_BLOQUE, audio.user);
         }
         waveOutWrite(audio.salida, h, sizeof(*h));
      }
      WaitForSingleObject(audio.evento, 20);
   }
   return 0;
}

bool
audio_start(audio_fill_fn fill, void *user)
{
   if (audio.salida) {
      return true;
   }
   WAVEFORMATEX f;
   memset(&f, 0, sizeof(f));
   f.wFormatTag = WAVE_FORMAT_PCM;
   f.nChannels = 2;
   f.nSamplesPerSec = AUDIO_HZ;
   f.wBitsPerSample = 16;
   f.nBlockAlign = (WORD)(f.nChannels * f.wBitsPerSample / 8);
   f.nAvgBytesPerSec = f.nSamplesPerSec * f.nBlockAlign;
   audio.evento = CreateEventA(NULL, FALSE, FALSE, NULL);
   if (waveOutOpen(&audio.salida, WAVE_MAPPER, &f, (DWORD_PTR)audio.evento, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
      say("sonido: no se pudo abrir la salida (waveOut)");
      CloseHandle(audio.evento);
      audio.salida = NULL;
      return false;
   }
   audio.fill = fill;
   audio.user = user;
   for (int i = 0; i < AUDIO_BUFERES; i++) {
      WAVEHDR *h = &audio.cabeceras[i];
      memset(h, 0, sizeof(*h));
      h->lpData = (LPSTR)audio.muestras[i];
      h->dwBufferLength = sizeof(audio.muestras[i]);
      waveOutPrepareHeader(audio.salida, h, sizeof(*h));
      h->dwFlags |= WHDR_DONE;
   }
   audio.seguir = 1;
   audio.hilo = CreateThread(NULL, 0, audio_hilo, NULL, 0, NULL);
   say("sonido: waveOut, %d Hz, %d buferes de %d cuadros", AUDIO_HZ, AUDIO_BUFERES, AUDIO_BLOQUE * AUDIO_POR_BUFER);
   return true;
}

void
audio_stop(void)
{
   if (!audio.salida) {
      return;
   }
   InterlockedExchange(&audio.seguir, 0);
   SetEvent(audio.evento);
   WaitForSingleObject(audio.hilo, INFINITE);
   CloseHandle(audio.hilo);
   waveOutReset(audio.salida);
   for (int i = 0; i < AUDIO_BUFERES; i++) {
      waveOutUnprepareHeader(audio.salida, &audio.cabeceras[i], sizeof(audio.cabeceras[i]));
   }
   waveOutClose(audio.salida);
   CloseHandle(audio.evento);
   audio.salida = NULL;
}
