# Cliente de yugictl (puerto 9113 de la PS5): ping / status / launch PPSA77002. Una orden por conexion.
# Uso: python -I yugictl_cliente.py ping|status|abrir   [--cargar: si no responde, lo carga con Payload Manager]
import socket, sys, time, urllib.parse, urllib.request
PS5 = open(__import__("os").path.join(__import__("os").path.dirname(__import__("os").path.abspath(__file__)), "..", "consola_ip.txt")).read().strip()   # IP: archivo local (no va al repo)
ELF = "/mnt/usb0/classicctl.elf"   # Payload Manager solo carga de su carpeta o del USB


def orden(linea, espera=200):
    with socket.create_connection((PS5, 9113), timeout=10) as c:
        c.settimeout(espera)
        c.sendall((linea + "\n").encode())
        datos = b""
        while not datos.endswith(b"\n"):
            parte = c.recv(512)
            if not parte:
                break
            datos += parte
    return datos.decode(errors="replace").strip()


def vivo():
    try:
        return orden("ping", 10).startswith("ok")
    except OSError:
        return False


if "--cargar" in sys.argv and not vivo():
    # Payload Manager (8084) carga el ELF desde la carpeta del juego
    url = "http://%s:8084/loadpayload:%s" % (PS5, urllib.parse.quote(ELF))
    print("cargando yugictl:", urllib.request.urlopen(url, timeout=30).status)
    for _ in range(30):
        time.sleep(1)
        if vivo():
            break
cmd = sys.argv[1]
if cmd == "ping":
    print(orden("ping", 10))
elif cmd == "status":
    print(orden("status", 30))
elif cmd == "abrir":
    print(orden("launch PPSA77002", 200))
