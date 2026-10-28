# Sala en línea de Yugioh Classic Remaster (Cloudflare Workers, plan gratis)

La sala **solo empareja**: recibe a los que buscan rival, junta a dos compatibles (misma versión del juego y mismo
catálogo de cartas; ELO parecido, con una ventana que se ensancha con la espera) y le da a cada uno los candidatos de
conexión del otro. El duelo va directo entre las dos consolas (P2P). Si la sala no responde, el juego busca rival por
la DHT de BitTorrent (`src/red/dht.h`), sin ningún servidor.

- Código: `src/index.js` (el Worker y el Durable Object `Sala`, una sola instancia "mundial").
- Configuración: `wrangler.toml` (sin IDs de cuenta: se usa la cuenta con la que se inicia sesión).
- Pruebas: `pruebas/prueba_sala.mjs` y `probar.sh` (en el contenedor `arch-ps5`).

## Por qué HTTP sin TLS

Los subdominios `*.workers.dev` atienden también `http://` (puerto 80) sin redirigir a https (se comprobó: la misma
respuesta por http y por https). Así la PS5 no necesita una biblioteca TLS (BearSSL/mbedTLS): el cliente es un HTTP/1.1
mínimo con sockets. Lo que viaja no es secreto (id aleatorio, nombre de duelista, ELO, IP:puerto de conexión, que de
todos modos el rival ve al conectarse P2P). **No actives "Always Use HTTPS"** si algún día pones un dominio propio.

## Desplegar (cuando tengas la cuenta)

1. Crea una cuenta gratis en <https://dash.cloudflare.com/sign-up> (plan **Free**; no hace falta tarjeta).
2. En el panel, entra a **Workers & Pages** una vez y elige tu subdominio `<algo>.workers.dev`.
3. En el contenedor (Node está en `/compartido/node`, nada se instala en Windows):
   ```bash
   docker exec -it arch-ps5 bash
   export PATH=/compartido/node/node-v22.11.0-linux-x64/bin:$PATH
   cd /compartido/wt/<rama>/remake-ps5/servidor/sala
   npm install
   npx wrangler login          # abre un enlace: cópialo al navegador de Windows y autoriza
   ```
   Si `wrangler login` no puede abrir el navegador dentro del contenedor, usa un token: en el panel, *My Profile →
   API Tokens → Create Token → plantilla "Edit Cloudflare Workers"*, y luego `export CLOUDFLARE_API_TOKEN=<token>`
   (no lo guardes en el repo).
4. Publica:
   ```bash
   npx wrangler deploy
   ```
   Wrangler crea el Durable Object (`new_sqlite_classes`, el único que permite el plan gratis) y muestra la URL,
   por ejemplo `https://yugioh-classic-sala.<algo>.workers.dev`.
5. Prueba que responde **por http**:
   ```bash
   curl http://yugioh-classic-sala.<algo>.workers.dev/v1/enLinea
   node pruebas/prueba_sala.mjs http://yugioh-classic-sala.<algo>.workers.dev
   ```
6. Dale la URL al juego: `red::SalaConfig::url` (por defecto `src/red/sala.h`, `URL_SALA`) o, para probar, la
   variable `YUGI_SALA=http://...` en el PC / la línea `sala=http://...` en `/data/yugioh_classic/red.txt` en la consola.

## Límites del plan gratis (aprox.)

- Workers: 100 000 pedidos por día. Un jugador buscando hace un pedido cada ~20 s (long-polling), unos 180 por hora.
- Durable Objects (SQLite): se cobra el tiempo que el objeto está activo; con un único objeto, aunque atienda a muchos
  a la vez, son como mucho 24 h de actividad al día, dentro del gratis.
- Si se pasa del límite, la sala devuelve error y el juego cae solo a la DHT.

## Protocolo

Texto plano, una línea `clave=valor` por campo (las claves repetidas, como `cand`, son listas).

| Ruta | Pide | Responde |
|---|---|---|
| `POST /v1/buscar` | `id` (hex aleatorio, 16-64), `nombre`, `elo`, `version`, `catalogo`, `plataforma` (`ps5`/`pc`), `cand`* | `estado=buscando` (o `emparejado`), `elo` (el de la sala manda) |
| `POST /v1/estado` | `id`, `espera` (ms, hasta 20000) | `estado=buscando\|emparejado\|desconocido`; emparejado: `sesion`, `anfitrion`, `rival_*`, `cand`* |
| `POST /v1/cancelar` | `id` | `estado=cancelado` |
| `GET /v1/enLinea` | — | `conectados`, `buscando`, `duelos` |
| `POST /v1/resultado` | `id`, `sesion`, `gane` (1/0) | `estado=pendiente\|contado\|descartado`, `elo` |

Toda respuesta trae también `conectados`, `buscando` y `duelos`. Candidato: `cand=<tipo>,<prioridad>,<dirección>`
con tipo `local|ipv6|upnp|stun|manual` y dirección `1.2.3.4:5000` o `[2001:db8::1]:5000`.

Reglas: anfitrión = el de id menor; el resultado cuenta solo si los dos lo reportan y coinciden (uno ganó, el otro
perdió); límite de 90 pedidos por minuto por IP (en memoria, la IP no se guarda); una búsqueda sin consultas en 45 s
se borra; el ELO de un id que no juega en 180 días se borra.
