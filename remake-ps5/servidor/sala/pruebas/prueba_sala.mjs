// Pruebas de la sala (rama classic-sala). Dos partes:
//  1. el Durable Object en memoria (almacenamiento falso y reloj falso): emparejamiento por version/catalogo y ELO,
//     ventana que se ensancha, pocos jugadores, cancelar, caducidad, resultado y ELO, limite por IP;
//  2. si se pasa una URL (node pruebas/prueba_sala.mjs http://127.0.0.1:8787), lo mismo de punta a punta contra
//     `wrangler dev` (el Worker real en workerd, local).
// Cada caso imprime "OK <id>" o "FALLO <id>: ..."; sale con 1 si hubo fallos.
import { Sala, emparejar, ventanaElo, eloNuevo, leerCampos, escribirCampos, candidatoValido, LIMITES } from "../src/index.js";

let fallos = 0, casos = 0;
function caso(id, cond, detalle = "") {
	casos++;
	if (cond) console.log(`OK ${id}`);
	else { fallos++; console.log(`FALLO ${id}: ${detalle}`); }
}

class AlmacenFalso {
	constructor() { this.m = new Map(); this.alarma = null; }
	async get(k) { return this.m.has(k) ? structuredClone(this.m.get(k)) : undefined; }
	async put(k, v) { this.m.set(k, structuredClone(v)); }
	async delete(k) { for (const x of [].concat(k)) this.m.delete(x); }
	async list({ prefix }) { return new Map([...this.m].filter(([k]) => k.startsWith(prefix))); }
	async getAlarm() { return this.alarma; }
	async setAlarm(t) { this.alarma = t; }
}

function salaFalsa() {
	const s = new Sala({ storage: new AlmacenFalso() }, {});
	let desfase = 0;   // reloj real (el long-polling espera de verdad) mas lo que la prueba adelanta
	s.ahora = () => Date.now() + desfase;
	s.avanzar = (ms) => { desfase += ms; };
	return s;
}

const hex = (n) => n.toString(16).padStart(16, "0");
function perfil(n, extra = {}) {
	return { id: hex(n), nombre: "Duelista " + n, elo: 1000, version: "1.0", catalogo: "goat1", plataforma: "ps5", cand: ["local,100,192.168.1." + (n % 250) + ":5000"], ...extra };
}

// pedido a la sala en memoria (o a la URL real)
async function pedir(destino, ruta, campos, ip = "1.1.1.1") {
	const body = campos ? escribirCampos(campos) : undefined;
	const req = { method: campos ? "POST" : "GET", headers: { "x-sala-ip": ip, "cf-connecting-ip": ip }, body };
	let r;
	if (typeof destino === "string") r = await fetch(destino + "/v1/" + ruta, req);
	else r = await destino.fetch(new Request("http://sala/v1/" + ruta, req));
	const f = leerCampos(await r.text());
	f.status = r.status;
	return f;
}

// ------------------------------------------------------------------------------------- 1. funciones puras
{
	const c = leerCampos("id=abc\ncand=local,1,1.2.3.4:5\ncand=ipv6,2,[::1]:7\nnombre=a=b\n");
	caso("CU-RED-SALA-TEXTO", c.id === "abc" && c.cand.length === 2 && c.nombre === "a=b", JSON.stringify(c));
	caso("CU-RED-SALA-CANDIDATO-VALIDO", candidatoValido("stun,50,8.8.8.8:1234") && candidatoValido("ipv6,9,[2001:db8::1]:9") &&
		!candidatoValido("raro,1,1.2.3.4:5") && !candidatoValido("local,1,hola:5"), "");
	caso("CU-RED-SALA-VENTANA-ELO", ventanaElo(0) === 100 && ventanaElo(10000) === 350 && ventanaElo(30000) === Infinity, "");
	const a = eloNuevo(1000, 1000, true), b = eloNuevo(1000, 1000, false);
	caso("CU-RED-SALA-ELO-FORMULA", a === 1016 && b === 984 && eloNuevo(1400, 1000, true) - 1400 < 16, `${a} ${b}`);
	const t = 0;
	const fila = [
		{ id: "a", elo: 1000, version: "1", catalogo: "x", desde: t },
		{ id: "b", elo: 1800, version: "1", catalogo: "x", desde: t },
		{ id: "c", elo: 1050, version: "1", catalogo: "x", desde: t },
		{ id: "d", elo: 1790, version: "1", catalogo: "x", desde: t },
	];
	const p = emparejar(fila, t).map((x) => x.map((j) => j.id).sort().join("")).sort();
	caso("CU-RED-SALA-EMPAREJA-ELO-CERCANO", p.join(",") === "ac,bd", p.join(","));
	const lejos = [
		{ id: "a", elo: 1000, version: "1", desde: 0 }, { id: "b", elo: 1800, version: "1", desde: 0 },
		{ id: "c", elo: 2600, version: "1", desde: 0 }, { id: "d", elo: 400, version: "1", desde: 0 },
	];
	caso("CU-RED-SALA-ELO-LEJOS-ESPERA", emparejar(lejos, 1000).length === 0 && emparejar(lejos, 31000).length === 2, "");
	const pocos = [{ id: "a", elo: 100, version: "1", desde: 0 }, { id: "b", elo: 2900, version: "1", desde: 0 }];
	caso("CU-RED-SALA-POCOS-PRIMERO", emparejar(pocos, 0).length === 1, "");
	const versiones = [{ id: "a", elo: 1000, version: "1", desde: 0 }, { id: "b", elo: 1000, version: "2", desde: 0 }];
	caso("CU-RED-SALA-VERSION-DISTINTA", emparejar(versiones, 99999).length === 0, "");
	const catalogos = [{ id: "a", elo: 1000, version: "1", catalogo: "x", desde: 0 }, { id: "b", elo: 1000, version: "1", catalogo: "y", desde: 0 }];
	caso("CU-RED-SALA-CATALOGO-DISTINTO", emparejar(catalogos, 99999).length === 0, "");
}

// ------------------------------------------------------------------------------------- 2. la sala (memoria o URL)
async function bateria(nombre, nueva, avanzar) {
	const pre = nombre === "memoria" ? "" : "-WORKER";
	// buscar dos compatibles: se emparejan con los candidatos del otro y un solo anfitrion
	let s = nueva();
	const base = 0x100 + Math.floor(Math.random() * 1e9);
	const A = perfil(base + 1), B = perfil(base + 2, { plataforma: "pc", cand: ["stun,50,203.0.113.7:4000", "ipv6,60,[2001:db8::7]:4000"] });
	let r = await pedir(s, "buscar", A);
	caso("CU-RED-SALA-BUSCAR" + pre, r.status === 200 && r.estado === "buscando" && r.elo === "1000", JSON.stringify(r));
	r = await pedir(s, "estado", { id: A.id });
	caso("CU-RED-SALA-ESTADO-BUSCANDO" + pre, r.estado === "buscando" && Number(r.buscando) >= 1, JSON.stringify(r));
	r = await pedir(s, "buscar", B);
	const ra = await pedir(s, "estado", { id: A.id, espera: 500 });
	const rb = await pedir(s, "estado", { id: B.id });
	caso("CU-RED-SALA-EMPAREJADO" + pre, ra.estado === "emparejado" && rb.estado === "emparejado" && ra.sesion === rb.sesion &&
		ra.rival_id === B.id && rb.rival_id === A.id, JSON.stringify([ra, rb]));
	caso("CU-RED-SALA-CANDIDATOS-RIVAL" + pre, (ra.cand || []).length === 2 && ra.cand[0] === B.cand[0] && rb.cand[0] === A.cand[0], JSON.stringify(ra.cand));
	caso("CU-RED-SALA-ANFITRION-UNICO" + pre, Number(ra.anfitrion) + Number(rb.anfitrion) === 1 && ra.anfitrion === "1", `${ra.anfitrion} ${rb.anfitrion}`);
	caso("CU-RED-SALA-CROSSPLAY" + pre, ra.rival_plataforma === "pc" && rb.rival_plataforma === "ps5", "");
	let en = await pedir(s, "enLinea");
	caso("CU-RED-SALA-EN-LINEA" + pre, Number(en.conectados) >= 2 && Number(en.duelos) >= 1, JSON.stringify(en));
	// resultado: los dos de acuerdo -> cuenta y cambia el ELO
	r = await pedir(s, "resultado", { id: A.id, sesion: ra.sesion, gane: 1 });
	caso("CU-RED-SALA-RESULTADO-PENDIENTE" + pre, r.estado === "pendiente", JSON.stringify(r));
	r = await pedir(s, "resultado", { id: B.id, sesion: ra.sesion, gane: 0 });
	caso("CU-RED-SALA-RESULTADO-CONTADO" + pre, r.estado === "contado" && r.elo === "984", JSON.stringify(r));
	r = await pedir(s, "resultado", { id: A.id, sesion: ra.sesion, gane: 1 });
	caso("CU-RED-SALA-ELO-GUARDADO" + pre, r.estado === "contado" && r.elo === "1016", JSON.stringify(r));
	// el ELO de la sala manda: vuelve a buscar diciendo 2900 y la sala responde 1016
	r = await pedir(s, "buscar", { ...A, elo: 2900 });
	caso("CU-RED-SALA-ELO-MANDA-SALA" + pre, r.elo === "1016", JSON.stringify(r));
	await pedir(s, "cancelar", { id: A.id });
	// resultado que no coincide (los dos dicen que ganaron): no cuenta
	const C = perfil(base + 3), D = perfil(base + 4);
	await pedir(s, "buscar", C);
	await pedir(s, "buscar", D);
	const rc = await pedir(s, "estado", { id: C.id });
	await pedir(s, "resultado", { id: C.id, sesion: rc.sesion, gane: 1 });
	r = await pedir(s, "resultado", { id: D.id, sesion: rc.sesion, gane: 1 });
	const rC = await pedir(s, "resultado", { id: C.id, sesion: rc.sesion, gane: 1 });
	caso("CU-RED-SALA-RESULTADO-NO-COINCIDE" + pre, r.estado === "descartado" && r.elo === "1000" && rC.elo === "1000", JSON.stringify(r));
	r = await pedir(s, "resultado", { id: perfil(base + 99).id, sesion: rc.sesion, gane: 1 });
	caso("CU-RED-SALA-RESULTADO-AJENO" + pre, r.status === 404, JSON.stringify(r));
	// version distinta no empareja
	const E = perfil(base + 5, { version: "1.0" }), F = perfil(base + 6, { version: "2.0" });
	await pedir(s, "buscar", E);
	await pedir(s, "buscar", F);
	r = await pedir(s, "estado", { id: E.id, espera: 300 });
	caso("CU-RED-SALA-VERSION-NO-EMPAREJA" + pre, r.estado === "buscando", JSON.stringify(r));
	// cancelar: deja de buscar y F (version 2.0) sigue solo
	r = await pedir(s, "cancelar", { id: E.id });
	const r2 = await pedir(s, "estado", { id: E.id });
	caso("CU-RED-SALA-CANCELAR" + pre, r.estado === "cancelado" && r2.estado === "desconocido", JSON.stringify(r2));
	await pedir(s, "cancelar", { id: F.id });
	// long-polling: A espera y B llega mientras tanto; A se despierta antes de la espera completa
	const G = perfil(base + 7, { version: "lp" }), H = perfil(base + 8, { version: "lp" });
	await pedir(s, "buscar", G);
	const t0 = Date.now();
	const espera = pedir(s, "estado", { id: G.id, espera: 8000 });
	setTimeout(() => pedir(s, "buscar", H), 400);
	r = await espera;
	caso("CU-RED-SALA-LONG-POLLING" + pre, r.estado === "emparejado" && Date.now() - t0 < 6000, `${r.estado} ${Date.now() - t0} ms`);
	// datos malos
	r = await pedir(s, "buscar", { id: "NO-HEX", version: "1" });
	caso("CU-RED-SALA-ID-INVALIDO" + pre, r.status === 400, JSON.stringify(r));
	// caducidad y limite por IP: solo en memoria (reloj falso)
	if (avanzar) {
		s = nueva();
		await pedir(s, "buscar", perfil(1, { version: "cad" }));
		avanzar(s, LIMITES.caducaBusqueda + 1000);
		r = await pedir(s, "estado", { id: perfil(1).id });
		caso("CU-RED-SALA-CADUCIDAD", r.estado === "desconocido" && r.buscando === "0", JSON.stringify(r));
		await pedir(s, "buscar", perfil(2, { version: "cad" }));
		avanzar(s, 30000);
		await pedir(s, "estado", { id: perfil(2).id });   // sigue vivo porque consulta
		avanzar(s, 30000);
		r = await pedir(s, "estado", { id: perfil(2).id });
		caso("CU-RED-SALA-CONSULTA-MANTIENE", r.estado === "buscando", JSON.stringify(r));
		s = nueva();
		let bloqueado = 0;
		for (let i = 0; i < LIMITES.pedidosPorMinuto + 10; i++) if ((await pedir(s, "enLinea", null, "9.9.9.9")).status === 429) bloqueado++;
		const otra = await pedir(s, "enLinea", null, "8.8.8.8");
		avanzar(s, 61000);
		const despues = await pedir(s, "enLinea", null, "9.9.9.9");
		caso("CU-RED-SALA-LIMITE-IP", bloqueado === 10 && otra.status === 200 && despues.status === 200, `${bloqueado}`);
		// la alarma borra sesiones viejas
		s = nueva();
		await pedir(s, "buscar", perfil(10));
		await pedir(s, "buscar", perfil(11));
		avanzar(s, LIMITES.caducaSesion + 1);
		const st = s.state.storage;
		const antes = (await st.list({ prefix: "ses:" })).size;
		await s.alarm();
		caso("CU-RED-SALA-ALARMA-LIMPIA", antes === 1 && (await st.list({ prefix: "ses:" })).size === 0, `${antes}`);
	}
}

await bateria("memoria", salaFalsa, (s, ms) => s.avanzar(ms));
const url = process.argv[2];
if (url) {
	const u = url.replace(/\/$/, "");
	const raiz = await fetch(u + "/");
	caso("CU-RED-SALA-WORKER-HTTP", raiz.status === 200 && (await raiz.text()).includes("sala"), String(raiz.status));
	await bateria("worker", () => u, null);
}
console.log(`${fallos ? "PRUEBA CON FALLOS" : "TODO OK"}: ${casos} casos, ${fallos} fallos`);
process.exit(fallos ? 1 : 0);
