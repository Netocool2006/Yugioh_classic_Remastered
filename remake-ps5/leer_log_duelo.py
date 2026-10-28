# Lee la bitacora del duelo (duelo_log.txt) que Yugioh Classic Remaster escribe en la PS5, por el FTP de ftpsrv (2121).
# SOLO LECTURA: usa unicamente SIZE y RETR (con REST); nunca sube, renombra ni borra nada en la consola.
# La IP sale de remake-ps5/consola_ip.txt (igual que subir_imagen.py); usuario anonymous / yugi.
#
#   python -I leer_log_duelo.py una            # descarga y muestra el log completo
#   python -I leer_log_duelo.py una -n 80      # solo las ultimas 80 lineas
#   python -I leer_log_duelo.py seguir         # como tail -f: cada ~3 s pide solo lo nuevo (Ctrl+C para salir)
#   python -I leer_log_duelo.py seguir -n 30   # empieza mostrando las ultimas 30 lineas y sigue
# Opciones: --anterior (duelo_log.anterior.txt), --filtro TEXTO (solo las lineas que lo contienen; p. ej. "CPU decide"),
#           --host IP, --puerto N, --archivo RUTA (por defecto /data/yugioh_classic/duelo_log.txt), --cada SEGUNDOS
# Formato de las lineas: remake-ps5/docs/duelo_log.md
import argparse, ftplib, io, os, sys, time

AQUI = os.path.dirname(os.path.abspath(__file__))
RUTA_DEFECTO = "/data/yugioh_classic/duelo_log.txt"
RUTA_ANTERIOR = "/data/yugioh_classic/duelo_log.anterior.txt"


def ip_consola():
    ip_txt = os.path.join(AQUI, "consola_ip.txt")
    if not os.path.exists(ip_txt):   # archivo local con la IP de la consola (no va al repo)
        sys.exit("falta %s con la IP de la PS5 (o usa --host)" % ip_txt)
    return open(ip_txt).read().strip()


def conectar(host, puerto):
    ftp = ftplib.FTP()
    ftp.connect(host, puerto, timeout=15)
    ftp.login("anonymous", "yugi")
    ftp.voidcmd("TYPE I")
    return ftp


def tam_remoto(ftp, ruta):
    """Tamano del archivo en la consola, o None si no existe (ftpsrv responde 213 con -1 cuando no existe)."""
    try:
        t = ftp.size(ruta)
        return None if t is None or t >= 1 << 63 else t
    except ftplib.all_errors:
        return None


def leer_desde(ftp, ruta, desde):
    """Los bytes de ruta desde el desplazamiento "desde" (RETR con REST)."""
    trozos = []
    ftp.retrbinary("RETR " + ruta, trozos.append, blocksize=1 << 16, rest=desde or None)
    return b"".join(trozos)


def lineas_completas(datos):
    """(lineas, bytes consumidos): solo las lineas terminadas en salto; el resto espera a la proxima lectura."""
    fin = datos.rfind(b"\n")
    if fin < 0:
        return [], 0
    texto = datos[: fin + 1].decode("utf-8", errors="replace")
    return texto.splitlines(), fin + 1


def mostrar(lineas, filtro):
    for l in lineas:
        if not filtro or filtro in l:
            print(l, flush=True)


def inicial(ftp, ruta, n, tam):
    """(lineas, posicion): las ultimas n lineas (o todas si n = 0) y el desplazamiento justo despues de la ultima completa."""
    if n <= 0:
        ls, usados = lineas_completas(leer_desde(ftp, ruta, 0))
        return ls, usados
    ventana = 64 * 1024
    while True:
        desde = max(0, tam - ventana)
        datos = leer_desde(ftp, ruta, desde)
        ini = 0
        if desde > 0:   # (la primera linea puede venir cortada)
            corte = datos.find(b"\n")
            ini = corte + 1 if corte >= 0 else len(datos)
        ls, usados = lineas_completas(datos[ini:])
        if len(ls) >= n or desde == 0:
            return ls[-n:], desde + ini + usados
        ventana *= 4


def main():
    ap = argparse.ArgumentParser(description="Lee (solo lectura) la bitacora del duelo de la PS5 por FTP")
    ap.add_argument("modo", nargs="?", default="una", choices=["una", "seguir"])
    ap.add_argument("-n", type=int, default=0, help="solo las ultimas N lineas (en 'seguir': las que se muestran al empezar)")
    ap.add_argument("--anterior", action="store_true", help="el log anterior (duelo_log.anterior.txt)")
    ap.add_argument("--filtro", default="", help="solo las lineas que contienen este texto")
    ap.add_argument("--host", default="")
    ap.add_argument("--puerto", type=int, default=2121)
    ap.add_argument("--archivo", default="")
    ap.add_argument("--cada", type=float, default=3.0, help="segundos entre consultas en 'seguir'")
    a = ap.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    host = a.host or ip_consola()
    ruta = a.archivo or (RUTA_ANTERIOR if a.anterior else RUTA_DEFECTO)
    print("# ftp://%s:%d%s (solo lectura)" % (host, a.puerto, ruta), file=sys.stderr)

    ftp = conectar(host, a.puerto)
    tam = tam_remoto(ftp, ruta)
    if a.modo == "una":
        if tam is None:
            sys.exit("no existe %s en la consola (el juego aun no escribio una bitacora, o esta en No)" % ruta)
        ls, _ = inicial(ftp, ruta, a.n, tam)
        mostrar(ls, a.filtro)
        ftp.quit()
        return

    # seguir: como tail -f. "pos" = hasta donde ya se imprimio; solo se piden los bytes nuevos (REST)
    pos = 0
    if tam is not None:
        ls, pos = inicial(ftp, ruta, a.n, tam)
        mostrar(ls, a.filtro)
    else:
        print("# todavia no hay bitacora; espero a que empiece un duelo...", file=sys.stderr)
    try:
        while True:
            time.sleep(a.cada)
            try:
                if ftp is None:
                    ftp = conectar(host, a.puerto)
                tam = tam_remoto(ftp, ruta)
                if tam is None:
                    pos = 0
                    continue
                if tam < pos:   # el archivo se roto o se reemplazo: de nuevo desde el principio
                    print("# ---- la bitacora empezo de nuevo (rotacion) ----", flush=True)
                    pos = 0
                if tam > pos:
                    datos = leer_desde(ftp, ruta, pos)
                    ls, usados = lineas_completas(datos)
                    mostrar(ls, a.filtro)
                    pos += usados
            except (OSError, EOFError, ftplib.Error) as e:
                print("# sin conexion (%s); reintento" % e, file=sys.stderr, flush=True)
                try:
                    if ftp:
                        ftp.close()
                except Exception:
                    pass
                ftp = None
    except KeyboardInterrupt:
        print("\n# fin", file=sys.stderr)


if __name__ == "__main__":
    main()
