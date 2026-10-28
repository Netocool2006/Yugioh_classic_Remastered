/*
 * Yugioh Classic Remaster - sala de emparejamiento en linea (rama classic-sala).
 *
 * Un Worker de Cloudflare (plan gratis) con un unico Durable Object ("mundial") que guarda la fila de busqueda. La sala
 * SOLO empareja e intercambia los candidatos de conexion (IP:puerto que cada consola cree que la alcanzan); el duelo
 * va directo entre las dos consolas (P2P), nunca pasa por aca.
 *
 * Protocolo (HTTP simple, sin TLS: la PS5 no necesita una biblioteca TLS). Pedidos y respuestas en texto, una linea
 * "clave=valor" por campo (las claves repetidas, como "cand", son listas). Rutas:
 *   POST /v1/buscar     id, nombre, elo, version, catalogo, plataforma, cand* -> estado=buscando (o emparejado) y elo
 *   POST /v1/estado     id, espera (ms, hasta 20000: long-polling)            -> estado=buscando|emparejado|desconocido
 *                       emparejado: sesion, anfitrion (1/0), rival_id, rival_nombre, rival_elo, rival_version,
 *                       rival_plataforma, cand* (los del rival)
 *   POST /v1/cancelar   id                                                    -> estado=cancelado
 *   GET  /v1/enLinea                                                          -> conectados, buscando, duelos
 *   POST /v1/resultado  id, sesion, gane (1/0)                                -> estado=pendiente|contado|descartado, elo
 * Todas las respuestas llevan tambien conectados/buscando/duelos (el contador del menu sale gratis).
 * Un candidato: "cand=<tipo>,<prioridad>,<direccion>" con tipo local|ipv6|upnp|stun|manual y direccion "1.2.3.4:5000"
 * o "[2001:db8::1]:5000".
 *
 * Nada de datos personales: el id es aleatorio (lo genera la consola), el nombre es el de duelista; la IP solo se usa
 * en memoria para el limite de pedidos y se olvida a los 2 minutos. En el almacenamiento queda solo el ELO por id (se
 * borra si no juega en 180 dias) y las sesiones de duelo hasta que se reporta el resultado (o 3 horas).
 */

export const LIMITES = {
	esperaMax: 20000,          // long-polling: lo maximo que /estado retiene un pedido
	caducaBusqueda: 45000,     // sin /estado en este tiempo, la busqueda se borra
	caducaEmparejado: 120000,  // el emparejado espera este tiempo a que el otro lo lea
	caducaSesion: 3 * 3600e3,  // una sesion sin resultado se borra a las 3 horas
	duracionDuelo: 3600e3,     // para contar "duelos en curso": sesiones de menos de 1 hora sin resultado
	vistoConectado: 120000,    // "conectados": ids vistos en los ultimos 2 minutos
	caducaElo: 180 * 86400e3,  // ELO de un id que no juega en 180 dias
	pedidosPorMinuto: 90,      // por IP
	maxCandidatos: 8,
	maxFila: 5000,
	eloInicial: 1000,
	factorK: 32,
};

// ------------------------------------------------------------------------------------- texto clave=valor
export function leerCampos(texto) {
	const campos = {};
	for (const linea of String(texto || "").split("\n")) {
		const l = linea.replace(/\r$/, "");
		const i = l.indexOf("=");
		if (i <= 0) continue;
		const k = l.slice(0, i).trim();
		const v = l.slice(i + 1);
		if (k === "cand") (campos.cand ||= []).push(v);
		else campos[k] = v;
	}
	return campos;
}

export function escribirCampos(obj) {
	let s = "";
	for (const [k, v] of Object.entries(obj)) {
		if (v === undefined || v === null) continue;
		if (Array.isArray(v)) for (const x of v) s += `${k}=${limpiar(x, 200)}\n`;
		else s += `${k}=${limpiar(v, 200)}\n`;
	}
	return s;
}

function limpiar(v, max) {
	return String(v).replace(/[\r\n\0]/g, " ").slice(0, max);
}

const TIPOS = new Set(["local", "ipv6", "upnp", "stun", "manual"]);
export function candidatoValido(c) {
	const p = String(c).split(",");
	if (p.length !== 3 || !TIPOS.has(p[0])) return false;
	if (!/^-?\d{1,6}$/.test(p[1])) return false;
	return /^(\d{1,3}(\.\d{1,3}){3}|\[[0-9a-fA-F:.]{2,45}\]):\d{1,5}$/.test(p[2]);
}

// ------------------------------------------------------------------------------------- ELO
export function eloEsperado(a, b) {
	return 1 / (1 + Math.pow(10, (b - a) / 400));
}
export function eloNuevo(a, b, gano, k = LIMITES.factorK) {
	return Math.round(a + k * ((gano ? 1 : 0) - eloEsperado(a, b)));
}

// ------------------------------------------------------------------------------------- emparejamiento
// ventana de ELO que se ensancha con la espera: 100 al empezar, +25 por segundo; a los 30 s cualquiera
export function ventanaElo(esperaMs) {
	if (esperaMs >= 30000) return Infinity;
	return 100 + 25 * (esperaMs / 1000);
}

// fila: [{id, elo, version, catalogo, desde}] en orden de llegada. Devuelve [[a, b], ...] (los emparejados salen
// de la lista en el orden en que se forman). Con pocos jugadores compatibles (3 o menos) se empareja el primero que
// aparezca, sin mirar el ELO; si no, cada uno con el compatible mas cercano en ELO dentro de las dos ventanas.
export function emparejar(fila, ahora) {
	const pares = [];
	const libres = fila.slice();
	const grupos = new Map();
	for (const j of libres) {
		const g = j.version + "|" + (j.catalogo || "");
		if (!grupos.has(g)) grupos.set(g, []);
		grupos.get(g).push(j);
	}
	for (const lista of grupos.values()) {
		const usados = new Set();
		const pocos = lista.length <= 3;
		for (let i = 0; i < lista.length; i++) {
			const a = lista[i];
			if (usados.has(a.id)) continue;
			let mejor = null, dif = Infinity;
			for (let j = i + 1; j < lista.length; j++) {
				const b = lista[j];
				if (usados.has(b.id) || b.id === a.id) continue;
				const d = Math.abs(a.elo - b.elo);
				if (!pocos) {
					const v = Math.min(ventanaElo(ahora - a.desde), ventanaElo(ahora - b.desde));
					if (d > v) continue;
				}
				if (pocos) { mejor = b; break; }
				if (d < dif) { mejor = b; dif = d; }
			}
			if (mejor) {
				usados.add(a.id);
				usados.add(mejor.id);
				pares.push([a, mejor]);
			}
		}
	}
	return pares;
}

// el anfitrion es el de id menor (la misma regla que la DHT: las dos consolas lo calculan igual)
export function esAnfitrion(miId, idRival) {
	return miId < idRival;
}

function idSesionNuevo() {
	const b = new Uint32Array(2);
	crypto.getRandomValues(b);
	const v = (BigInt(b[0]) << 32n) | BigInt(b[1]);
	return (v === 0n ? 1n : v).toString();
}

function respuesta(obj, status = 200) {
	return new Response(escribirCampos(obj), {
		status,
		headers: { "content-type": "text/plain; charset=utf-8", "cache-control": "no-store" },
	});
}

// ------------------------------------------------------------------------------------- el Worker
export default {
	async fetch(request, env) {
		const url = new URL(request.url);
		if (url.pathname === "/" || url.pathname === "/v1") {
			return new Response("Yugioh Classic Remaster - sala\n", { headers: { "content-type": "text/plain" } });
		}
		if (!url.pathname.startsWith("/v1/")) return respuesta({ error: "ruta" }, 404);
		const stub = env.SALA.get(env.SALA.idFromName("mundial"));
		// la IP solo viaja al objeto para el limite de pedidos (no se guarda)
		const ip = request.headers.get("cf-connecting-ip") || "local";
		const h = new Headers(request.headers);
		h.set("x-sala-ip", ip);
		return stub.fetch(new Request(request.url, { method: request.method, headers: h, body: request.method === "POST" ? await request.text() : undefined }));
	},
};

// ------------------------------------------------------------------------------------- el Durable Object
export class Sala {
	constructor(state, env) {
		this.state = state;
		this.env = env;
		this.fila = new Map();       // id -> {id, nombre, elo, version, catalogo, plataforma, cand, desde, visto}
		this.parejas = new Map();    // id -> {sesion, rival:{...}, anfitrion, desde}
		this.esperas = new Map();    // id -> [resolver, ...] (long-polling)
		this.vistos = new Map();     // id -> ultima vez
		this.ips = new Map();        // ip -> {ventana, cuenta}
		this.duelos = new Map();     // sesion -> desde (copia en memoria para contar)
		this.cargado = false;
		this.ahora = () => Date.now();
	}

	async cargar() {
		if (this.cargado) return;
		this.cargado = true;
		const ses = await this.state.storage.list({ prefix: "ses:" });
		for (const [k, v] of ses) if (!v.cerrada) this.duelos.set(k.slice(4), v.desde);
		const alarma = await this.state.storage.getAlarm();
		if (alarma === null) await this.state.storage.setAlarm(Date.now() + 600000);
	}

	limiteIp(ip) {
		const t = this.ahora();
		let r = this.ips.get(ip);
		if (!r || t - r.ventana > 60000) { r = { ventana: t, cuenta: 0 }; this.ips.set(ip, r); }
		r.cuenta++;
		return r.cuenta <= LIMITES.pedidosPorMinuto;
	}

	contadores() {
		const t = this.ahora();
		let conectados = 0, duelos = 0;
		for (const [id, v] of this.vistos) {
			if (t - v > LIMITES.vistoConectado) this.vistos.delete(id);
			else conectados++;
		}
		for (const [s, d] of this.duelos) if (t - d < LIMITES.duracionDuelo) duelos++;
		return { conectados, buscando: this.fila.size, duelos };
	}

	limpiar() {
		const t = this.ahora();
		for (const [id, j] of this.fila) if (t - j.visto > LIMITES.caducaBusqueda) this.fila.delete(id);
		for (const [id, p] of this.parejas) if (t - p.desde > LIMITES.caducaEmparejado) this.parejas.delete(id);
		for (const [ip, r] of this.ips) if (t - r.ventana > 120000) this.ips.delete(ip);
		for (const [s, d] of this.duelos) if (t - d > LIMITES.caducaSesion) this.duelos.delete(s);
	}

	async eloDe(id) {
		const r = await this.state.storage.get("elo:" + id);
		return r ? r.elo : null;
	}

	async intentarEmparejar() {
		const t = this.ahora();
		const pares = emparejar([...this.fila.values()], t);
		for (const [a, b] of pares) {
			const sesion = idSesionNuevo();
			this.fila.delete(a.id);
			this.fila.delete(b.id);
			const ficha = (j) => ({ id: j.id, nombre: j.nombre, elo: j.elo, version: j.version, plataforma: j.plataforma, cand: j.cand });
			this.parejas.set(a.id, { sesion, rival: ficha(b), anfitrion: esAnfitrion(a.id, b.id), desde: t });
			this.parejas.set(b.id, { sesion, rival: ficha(a), anfitrion: esAnfitrion(b.id, a.id), desde: t });
			this.duelos.set(sesion, t);
			await this.state.storage.put("ses:" + sesion, { a: a.id, b: b.id, eloA: a.elo, eloB: b.elo, desde: t, rep: {} });
			this.despertar(a.id);
			this.despertar(b.id);
		}
	}

	despertar(id) {
		const l = this.esperas.get(id);
		if (!l) return;
		this.esperas.delete(id);
		for (const r of l) r();
	}

	respuestaEstado(id, extra = {}) {
		const c = this.contadores();
		const p = this.parejas.get(id);
		if (p) {
			return respuesta({
				estado: "emparejado", sesion: p.sesion, anfitrion: p.anfitrion ? 1 : 0,
				rival_id: p.rival.id, rival_nombre: p.rival.nombre, rival_elo: p.rival.elo,
				rival_version: p.rival.version, rival_plataforma: p.rival.plataforma, cand: p.rival.cand, ...extra, ...c,
			});
		}
		if (this.fila.has(id)) return respuesta({ estado: "buscando", ...extra, ...c });
		return respuesta({ estado: "desconocido", ...extra, ...c });
	}

	async fetch(request) {
		await this.cargar();
		const url = new URL(request.url);
		const ip = request.headers.get("x-sala-ip") || "local";
		if (!this.limiteIp(ip)) return respuesta({ error: "demasiados pedidos" }, 429);
		this.limpiar();
		const ruta = url.pathname.slice(4);
		if (ruta === "enLinea") return respuesta(this.contadores());
		if (request.method !== "POST") return respuesta({ error: "metodo" }, 405);
		const f = leerCampos(await request.text());
		const id = String(f.id || "");
		if (!/^[0-9a-f]{16,64}$/.test(id)) return respuesta({ error: "id" }, 400);
		this.vistos.set(id, this.ahora());

		if (ruta === "buscar") {
			const version = limpiar(f.version || "", 40);
			if (!version) return respuesta({ error: "version" }, 400);
			if (this.fila.size >= LIMITES.maxFila && !this.fila.has(id)) return respuesta({ error: "sala llena" }, 503);
			const cand = (f.cand || []).filter(candidatoValido).slice(0, LIMITES.maxCandidatos);
			const guardado = await this.eloDe(id);
			let elo = parseInt(f.elo, 10);
			if (!Number.isFinite(elo)) elo = LIMITES.eloInicial;
			elo = guardado ?? Math.max(100, Math.min(3000, elo));   // el de la sala manda sobre el que dice la consola
			this.parejas.delete(id);
			const previo = this.fila.get(id);
			this.fila.set(id, {
				id, nombre: limpiar(f.nombre || "Duelista", 24), elo, version, catalogo: limpiar(f.catalogo || "", 40),
				plataforma: (f.plataforma === "pc" ? "pc" : "ps5"), cand,
				desde: previo ? previo.desde : this.ahora(), visto: this.ahora(),
			});
			await this.intentarEmparejar();
			return this.respuestaEstado(id, { elo });
		}

		if (ruta === "estado") {
			const j = this.fila.get(id);
			if (j) j.visto = this.ahora();
			await this.intentarEmparejar();
			const espera = Math.max(0, Math.min(LIMITES.esperaMax, parseInt(f.espera, 10) || 0));
			if (j && !this.parejas.has(id) && espera > 0) {
				// long-polling: se retiene hasta que aparezca pareja o se cumpla la espera; cada 2 s se reintenta
				// emparejar (la ventana de ELO se ensancha con el tiempo aunque no llegue nadie nuevo)
				const fin = this.ahora() + espera;
				while (this.fila.has(id) && !this.parejas.has(id) && this.ahora() < fin) {
					await new Promise((res) => {
						const l = this.esperas.get(id) || [];
						l.push(res);
						this.esperas.set(id, l);
						setTimeout(res, Math.min(2000, Math.max(1, fin - this.ahora())));
					});
					const k = this.fila.get(id);
					if (k) k.visto = this.ahora();
					await this.intentarEmparejar();
				}
			}
			return this.respuestaEstado(id);
		}

		if (ruta === "cancelar") {
			this.fila.delete(id);
			this.parejas.delete(id);
			this.despertar(id);
			return respuesta({ estado: "cancelado", ...this.contadores() });
		}

		if (ruta === "resultado") {
			const sesion = String(f.sesion || "");
			if (!/^\d{1,20}$/.test(sesion)) return respuesta({ error: "sesion" }, 400);
			const s = await this.state.storage.get("ses:" + sesion);
			if (!s || (s.a !== id && s.b !== id)) return respuesta({ error: "sesion desconocida" }, 404);
			if (s.cerrada) return respuesta({ estado: s.contada ? "contado" : "descartado", elo: (await this.eloDe(id)) ?? (s.a === id ? s.eloA : s.eloB), ...this.contadores() });
			s.rep[id] = f.gane === "1" ? 1 : 0;
			const ra = s.rep[s.a], rb = s.rep[s.b];
			let estado = "pendiente";
			if (ra !== undefined && rb !== undefined) {
				s.cerrada = true;
				this.duelos.delete(sesion);
				if (ra + rb === 1) {   // uno gano y el otro perdio: cuenta
					const ea = (await this.eloDe(s.a)) ?? s.eloA, eb = (await this.eloDe(s.b)) ?? s.eloB;
					const t = this.ahora();
					await this.state.storage.put("elo:" + s.a, { elo: eloNuevo(ea, eb, ra === 1), t });
					await this.state.storage.put("elo:" + s.b, { elo: eloNuevo(eb, ea, rb === 1), t });
					s.contada = true;
					estado = "contado";
				} else estado = "descartado";   // no coinciden: no cuenta
			}
			await this.state.storage.put("ses:" + sesion, s);
			const elo = (await this.eloDe(id)) ?? (s.a === id ? s.eloA : s.eloB);
			return respuesta({ estado, elo, ...this.contadores() });
		}
		return respuesta({ error: "ruta" }, 404);
	}

	// cada 10 minutos: borra sesiones viejas y ELO de ids que no juegan hace 180 dias
	async alarm() {
		const t = this.ahora();
		const ses = await this.state.storage.list({ prefix: "ses:" });
		const borrar = [];
		for (const [k, v] of ses) if (t - v.desde > LIMITES.caducaSesion) borrar.push(k);
		const elos = await this.state.storage.list({ prefix: "elo:" });
		for (const [k, v] of elos) if (t - v.t > LIMITES.caducaElo) borrar.push(k);
		for (let i = 0; i < borrar.length; i += 128) await this.state.storage.delete(borrar.slice(i, i + 128));
		await this.state.storage.setAlarm(t + 600000);
	}
}
