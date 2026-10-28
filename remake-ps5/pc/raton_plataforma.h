/*
 * Yugioh Classic Remaster (Windows): el raton y el ultimo dispositivo de entrada, de la plataforma al juego.
 * Lo implementa plataforma_windows.c; lo usa el arranque (main_windows.cpp: manda los mensajes de la ventana) y el juego
 * (yugireload.cpp, solo con _WIN32: raton_tomar una vez por cuadro). La logica de apuntar esta en src/raton.h.
 */
#ifndef RATON_PLATAFORMA_H
#define RATON_PLATAFORMA_H

#ifdef __cplusplus
extern "C" {
#endif

#define DISPOSITIVO_PLAYSTATION 0
#define DISPOSITIVO_TECLADO 1
#define DISPOSITIVO_XBOX 2

struct raton_estado {
   float x, y;            /* px del area de la ventana (negativo: el puntero esta fuera) */
   float ancho, alto;     /* tamano del area de la ventana, en px */
   int movido;            /* se movio desde el cuadro anterior */
   int izq, der, centro;  /* recien apretados en este cuadro */
   int rueda;             /* muescas desde el cuadro anterior (+ = hacia arriba) */
   int visible;           /* el puntero se ve (se oculta unos segundos despues de usar mando o teclado) */
   int dispositivo;       /* DISPOSITIVO_*: el ultimo con el que se jugo */
};

/* un mensaje de la ventana (WM_MOUSEMOVE, WM_*BUTTONDOWN, WM_MOUSEWHEEL, WM_MOUSELEAVE); 1 = lo atendio */
int raton_mensaje(void *hwnd, unsigned msg, unsigned long long wparam, long long lparam);
/* WM_SETCURSOR en el area de la ventana: 1 = el puntero esta oculto (la ventana ya puso "sin cursor") */
int raton_cursor_oculto(void);
/* el estado de este cuadro (y limpia los recien apretados) */
void raton_tomar(struct raton_estado *r);

#ifdef __cplusplus
}
#endif

#endif
