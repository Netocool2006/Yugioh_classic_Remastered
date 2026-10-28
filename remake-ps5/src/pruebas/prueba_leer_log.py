# Prueba de leer_log_duelo.py contra un servidor FTP FALSO local (nunca la PS5): sirve un duelo_log.txt de una carpeta,
# anota cada comando que recibe y comprueba que
#   - "una" muestra el log completo y con -n solo las ultimas lineas;
#   - "seguir" imprime lo nuevo (con REST, sin repetir), no imprime una linea a medias, y reengancha si el archivo se rota;
#   - el lector SOLO usa USER/PASS/TYPE/SIZE/PASV/REST/RETR/QUIT: nunca STOR, DELE, RNFR/RNTO, MKD, APPE...
# Uso: python3 -I prueba_leer_log.py   (segundos; sin dependencias)
import os, socket, socketserver, subprocess, sys, tempfile, threading, time

AQUI = os.path.dirname(os.path.abspath(__file__))
LECTOR = os.path.join(AQUI, "..", "..", "leer_log_duelo.py")
PERMITIDOS = {"USER", "PASS", "TYPE", "SIZE", "PASV", "REST", "RETR", "QUIT", "NOOP"}
comandos = []
RAIZ = tempfile.mkdtemp()


class Manejador(socketserver.StreamRequestHandler):
    def responder(self, t):
        self.wfile.write((t + "\r\n").encode())
        self.wfile.flush()

    def handle(self):
        try:
            self.sesion()
        except OSError:   # (el cliente se corto: Ctrl+C del 'seguir')
            pass

    def sesion(self):
        self.responder("220 ftp falso")
        rest = 0
        pasivo = None
        while True:
            linea = self.rfile.readline().decode(errors="replace").strip()
            if not linea:
                return
            cmd, _, arg = linea.partition(" ")
            cmd = cmd.upper()
            comandos.append(cmd)
            if cmd == "USER": self.responder("331 clave")
            elif cmd == "PASS": self.responder("230 ok")
            elif cmd == "TYPE": self.responder("200 ok")
            elif cmd == "SIZE":
                ruta = os.path.join(RAIZ, arg.lstrip("/").replace("/", os.sep))
                self.responder("213 %d" % os.path.getsize(ruta) if os.path.exists(ruta) else "550 no existe")
            elif cmd == "REST":
                rest = int(arg)
                self.responder("350 ok")
            elif cmd == "PASV":
                pasivo = socket.socket()
                pasivo.bind(("127.0.0.1", 0))
                pasivo.listen(1)
                p = pasivo.getsockname()[1]
                self.responder("227 Entering Passive Mode (127,0,0,1,%d,%d)" % (p >> 8, p & 255))
            elif cmd == "RETR":
                ruta = os.path.join(RAIZ, arg.lstrip("/").replace("/", os.sep))
                if not os.path.exists(ruta) or pasivo is None:
                    self.responder("550 no")
                    continue
                self.responder("150 abriendo")
                c, _ = pasivo.accept()
                with open(ruta, "rb") as f:
                    f.seek(rest)
                    c.sendall(f.read())
                c.close()
                pasivo.close()
                pasivo, rest = None, 0
                self.responder("226 listo")
            elif cmd == "QUIT":
                self.responder("221 chao")
                return
            else:
                self.responder("502 no")


class Servidor(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


fallos = 0


def falla(m):
    global fallos
    fallos += 1
    print("FALLO:", m)


srv = Servidor(("127.0.0.1", 0), Manejador)
puerto = srv.server_address[1]
threading.Thread(target=srv.serve_forever, daemon=True).start()
carpeta = os.path.join(RAIZ, "data", "yugioh_classic")
os.makedirs(carpeta)
log = os.path.join(carpeta, "duelo_log.txt")
base = ["00:%02d [T%d Principal1 %s] linea numero %d con acentos: Dragón Bebé" % (k % 60, 1 + k // 10, "J" if k % 2 == 0 else "CPU", k) for k in range(200)]
with open(log, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(base) + "\n")


def correr(*args):
    return subprocess.run([sys.executable, "-I", LECTOR, "--host", "127.0.0.1", "--puerto", str(puerto), *args], capture_output=True, timeout=30)


# una vez: todo
r = correr("una")
sal = r.stdout.decode("utf-8").splitlines()
if r.returncode != 0 or sal != base: falla("'una' no muestra el log completo (%d lineas de %d, rc %d)" % (len(sal), len(base), r.returncode))
# una vez con -n
r = correr("una", "-n", "7")
sal = r.stdout.decode("utf-8").splitlines()
if sal != base[-7:]: falla("'una -n 7' no muestra las ultimas 7 (%r)" % sal[-2:])
# filtro
r = correr("una", "--filtro", "CPU")
if r.stdout.decode("utf-8").splitlines() != [l for l in base if "CPU" in l]: falla("--filtro no filtra")
# un archivo que no existe
r = correr("una", "--anterior")
if r.returncode == 0: falla("sin archivo anterior debia avisar con codigo distinto de 0")

# seguir: arranca mostrando las ultimas 3, luego llegan lineas nuevas (una a medias), y una rotacion
p = subprocess.Popen([sys.executable, "-I", LECTOR, "seguir", "-n", "3", "--cada", "0.3", "--host", "127.0.0.1", "--puerto", str(puerto)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
time.sleep(1.2)
with open(log, "a", encoding="utf-8", newline="\n") as f:
    f.write("nueva 1\nnueva 2\nmedia")   # (la ultima, sin salto: no debe salir todavia)
    f.flush()
time.sleep(1.2)
with open(log, "a", encoding="utf-8", newline="\n") as f:
    f.write(" linea completa\nnueva 4\n")
time.sleep(1.2)
with open(log, "w", encoding="utf-8", newline="\n") as f:   # rotacion: archivo nuevo mas chico
    f.write("DUELO NUEVO\nprimera del nuevo\n")
time.sleep(1.5)
p.terminate()
try:
    salida, err = p.communicate(timeout=10)
except subprocess.TimeoutExpired:
    p.kill()
    salida, err = p.communicate()
esperado = base[-3:] + ["nueva 1", "nueva 2", "media linea completa", "nueva 4", "# ---- la bitacora empezo de nuevo (rotacion) ----", "DUELO NUEVO", "primera del nuevo"]
sal = salida.decode("utf-8", errors="replace").splitlines()
if sal != esperado:
    falla("'seguir' imprimio otra cosa.\n   esperado: %r\n   obtenido: %r" % (esperado, sal))

# solo lectura
malos = [c for c in comandos if c not in PERMITIDOS]
if malos: falla("el lector uso comandos que no son de lectura: %s" % sorted(set(malos)))
print("comandos FTP usados: %s" % " ".join(sorted(set(comandos))))
srv.shutdown()
print("PRUEBA OK (0 fallos)" if not fallos else "PRUEBA CON FALLOS (%d)" % fallos)
sys.exit(1 if fallos else 0)
