#pragma once
// La vista en espejo del duelo en red (rama classic-en-linea). El motor no es simetrico entre lados: en las dos maquinas
// el anfitrion es el lado 0 y el invitado el lado 1 (duelo_red.h). La mesa, el mando y las pantallas suponen que el
// jugador es el lado 0; para el invitado el juego muestra una COPIA del duelo con los lados cambiados (sus cartas abajo,
// sus preguntas y sus sucesos como "del jugador") y, al jugar, devuelve la jugada al lado real con espejoJugada.
// La copia es solo para mirar: nunca se aplica nada sobre ella (las jugadas van a DueloRed::jugar).
// Se cambian los dos Lado, el activo, el ganador, los sucesos, la pregunta, la ventana, la cadena, el ataque en curso y
// los arreglos por lado del motor de efectos (lo que consultan los "jugables" y los menus del mando).
#include "../ia.h"

#include <utility>

namespace red {

namespace detalle_espejo {
inline int otro(int lado) { return lado == 0 ? 1 : lado == 1 ? 0 : lado; }
template <class T> inline void cambiar(std::array<T, 2>& a) { std::swap(a[0], a[1]); }
inline void lugar(ia::Lugar& g) { g.lado = otro(g.lado); }
inline void lugares(std::vector<ia::Lugar>& v) { for (ia::Lugar& g : v) lugar(g); }
inline void ventana(ia::Ventana& v) { v.lado = otro(v.lado); }
inline void eslabon(ia::Eslabon& e)
{
	e.lado = otro(e.lado);
	lugar(e.fuente);
	lugares(e.obj);
	lugares(e.elegidos);
	cambiar(e.redirige);
	ventana(e.ventana);
}
}  // namespace detalle_espejo

// el duelo visto desde el otro lado (el lado 1 pasa a ser el 0)
inline ia::Duelo espejoDuelo(const ia::Duelo& d)
{
	using namespace detalle_espejo;
	ia::Duelo r = d;
	r.alAplicar = nullptr;
	r.ladoVista = 0;
	std::swap(r.l[0], r.l[1]);
	for (ia::Lado& x : r.l)
		for (ia::Monstruo& m : x.m)
			if (m.duenno >= 0) m.duenno = otro(m.duenno);
	r.activo = otro(r.activo);
	if (r.ganador == 0 || r.ganador == 1) r.ganador = otro(r.ganador);
	cambiar(r.preguntarSiempre);
	for (ia::Suceso& s : r.sucesos) s.lado = otro(s.lado);
	ia::EstadoEfectos& e = r.ef;
	for (ia::Eslabon& x : e.cadena) eslabon(x);
	for (ia::Eslabon& x : e.disparos) eslabon(x);
	e.preg.lado = otro(e.preg.lado);
	lugares(e.preg.cands);
	ventana(e.ventana);
	ventana(e.ventanaTras);
	e.ataque.lado = otro(e.ataque.lado);
	e.repeticion.lado = otro(e.repeticion.lado);
	e.calculo.yo = otro(e.calculo.yo);
	e.calculo.otro = otro(e.calculo.otro);
	cambiar(e.combateId);
	cambiar(e.combateUid);
	cambiar(e.combateLado);
	for (int& x : e.combateLado) x = otro(x);
	for (auto& p : e.prestadas) p.lado = otro(p.lado);
	for (auto* v : { &e.espadaLuz, &e.capsulas, &e.colapsos, &e.puerta, &e.agujero, &e.m2VuelveEspera })
		for (auto& x : *v) x.lado = otro(x.lado);
	for (auto& x : e.nivelesMano) x.lado = otro(x.lado);
	for (int& x : e.manticoras) x = otro(x);
	for (auto& x : e.m2Virus) x.first = otro(x.first);
	for (auto& x : e.m2Necrofear) x.first = otro(x.first);
	// los arreglos por lado
	cambiar(e.dannoPend);
	cambiar(e.dannoPendBatalla);
	cambiar(e.sinDannoBatalla);
	cambiar(e.sinDestruirBatalla);
	cambiar(e.castigos);
	cambiar(e.porTurno);
	cambiar(e.sinDannoHasta);
	cambiar(e.porDuelo);
	cambiar(e.destruidosBatalla);
	cambiar(e.destruidosCampo);
	cambiar(e.desterradosCampo);
	cambiar(e.behemothUsado);
	cambiar(e.sinDannoCombate);
	cambiar(e.fakeTrap);
	cambiar(e.saltaBatalla);
	cambiar(e.descartaRobo);
	cambiar(e.revivalJam);
	cambiar(e.maliceDoll);
	cambiar(e.mirarArriba);
	cambiar(e.vampiroLord);
	cambiar(e.sinMagiasHasta);
	cambiar(e.rideFin);
	cambiar(e.sinDannoBatallaHasta);
	cambiar(e.bajaNivel);
	cambiar(e.bajaNivelTurno);
	cambiar(e.fenix);
	cambiar(e.calavera);
	cambiar(e.templo);
	cambiar(e.sinTrampasTurno);
	cambiar(e.finAbsoluto);
	cambiar(e.cretinos);
	cambiar(e.cuentaFinal);
	cambiar(e.zorros);
	cambiar(e.meteoros);
	cambiar(e.michizure);
	cambiar(e.saltaRobo);
	cambiar(e.sebek);
	cambiar(e.sinAtaqueTurno);
	cambiar(e.sellados);
	cambiar(e.dannoRecibido);
	cambiar(e.repiteDado);
	cambiar(e.vierteTurno);
	cambiar(e.shabti);
	cambiar(e.tipoLuz);
	cambiar(e.limite7);
	cambiar(e.pagoTurno);
	cambiar(e.pagado);
	cambiar(e.tipoRegulado);
	cambiar(e.robosEfecto);
	cambiar(e.robosEfectoTurno);
	cambiar(e.apropiar);
	cambiar(e.roboEnEslabon);
	cambiar(e.robado);
	cambiar(e.robadoTurno);
	cambiar(e.magiaPerdida);
	cambiar(e.trampaPerdida);
	cambiar(e.magiasPerdidas);
	cambiar(e.dTribu);
	cambiar(e.sinEspera);
	cambiar(e.tesoros);
	cambiar(e.dora);
	cambiar(e.theinenTurno);
	cambiar(e.negCombate);
	cambiar(e.monedaTurno);
	cambiar(e.eliminacion);
	cambiar(e.devuelto);
	cambiar(e.makyura);
	cambiar(e.sinMP1);
	cambiar(e.pasoEstrecho);
	cambiar(e.cebos);
	cambiar(e.agujeroBlanco);
	cambiar(e.kaminote);
	cambiar(e.sinInvocarTurno);
	cambiar(e.rejuvenecer);
	cambiar(e.dragones);
	cambiar(e.dragonesTurno);
	cambiar(e.m2SinInvocar);
	cambiar(e.m2Invoco);
	cambiar(e.m2SinPosicion);
	cambiar(e.m2SaltaTurno);
	cambiar(e.m2DiosaVeto);
	cambiar(e.m2Oricalco);
	return r;
}

// el duelo como lo ve el lado 'yo' (yo = 0: tal cual)
inline ia::Duelo vistaDe(const ia::Duelo& d, int yo) { return yo == 1 ? espejoDuelo(d) : d; }

}  // namespace red
