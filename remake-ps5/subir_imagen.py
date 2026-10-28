# Sube la imagen del titulo (PPSA77002.exfat, un solo archivo) a /data/homebrew/ de la PS5 por el FTP de ftpsrv (2121).
# Reanudable: sube a "<nombre>.part" (ShadowMount no lo toma por imagen) y, si se corta, la siguiente ejecucion sigue
# desde donde quedo con REST (probado en esta consola: STOR con REST continua el archivo y RETR con REST lee desde
# el desplazamiento). Al terminar compara el tamano y renombra .part -> .exfat; ShadowMount+ la monta en su siguiente
# escaneo (cada 15 s, espera 10 s de estabilidad). Nunca borra nada: si ya hay una imagen en la consola, se detiene
# antes de subir y pide borrarla a mano (en la PS5 debe quedar una sola imagen de Yugioh).
# Velocidad medida (oct 2026, 300 MB): 10-11 MB/s -> la imagen de ~4 GB tarda ~6-7 min.
# Uso: python -I subir_imagen.py [D:\Docker\compartido\ps5\PPSA77002.exfat] [--simular]
import ftplib, os, sys, time

AQUI = os.path.dirname(os.path.abspath(__file__))
ip_txt = os.path.join(AQUI, "consola_ip.txt")
if not os.path.exists(ip_txt):   # archivo local con la IP de la consola (no va al repo)
    sys.exit("falta %s con la IP de la PS5" % ip_txt)
PS5 = open(ip_txt).read().strip()
PUERTO = 2121
args = [a for a in sys.argv[1:] if not a.startswith("--")]
local = os.path.abspath(args[0] if args else r"D:\Docker\compartido\ps5\PPSA77002.exfat")
simular = "--simular" in sys.argv
nombre = os.path.basename(local)
assert nombre.startswith("PPSA77002") and nombre.endswith(".exfat"), "este repo solo sube la imagen de PPSA77002"
destino = "/data/homebrew/" + nombre
parcial = destino + ".part"
tam = os.path.getsize(local)
print("ftp://%s:%d%s <- %s (%.2f GB)" % (PS5, PUERTO, destino, local, tam / 1e9))
if simular:
    sys.exit(0)


def tam_remoto(ftp, ruta):
    try:
        t = ftp.size(ruta)
        # ftpsrv responde "213 18446744073709551615" (-1) cuando el archivo no existe
        return None if t is None or t >= 1 << 63 else t
    except ftplib.all_errors:
        return None


for intento in range(1, 31):
    try:
        ftp = ftplib.FTP()
        ftp.connect(PS5, PUERTO, timeout=60)
        ftp.login("anonymous", "yugi")
        ftp.voidcmd("TYPE I")
        if tam_remoto(ftp, destino) == tam and tam_remoto(ftp, parcial) is None:
            print("ya estaba subida y completa")
            ftp.quit(); break
        # el usuario quiere UNA sola imagen de Yugioh en la PS5: si queda otra (.anterior o _v2.._v9, los nombres usados cuando
        # ShadowMount se salto la ruta tras 3 reintentos), se borra a mano antes
        viejas = [r for r in (destino, destino + ".anterior", *(destino[:-6] + "_v%d.exfat" % n for n in range(2, 10))) if tam_remoto(ftp, r) is not None]
        if viejas:
            sys.exit("ya hay en la consola %s: borrala a mano (con el juego cerrado) y repite" % " y ".join(viejas))
        hecho = tam_remoto(ftp, parcial) or 0
        if hecho > tam:
            sys.exit("%s en la consola es mas grande que la imagen local: borralo a mano y repite" % parcial)
        if hecho < tam:
            print("intento %d: desde %.2f GB" % (intento, hecho / 1e9))
            t0, base = time.time(), hecho
            with open(local, "rb") as f:
                f.seek(hecho)

                def avance(b):
                    global hecho
                    hecho += len(b)
                    v = (hecho - base) / 1e6 / max(time.time() - t0, 1)
                    print("\r   %5.1f%%  %.2f GB  %.1f MB/s" % (100 * hecho / tam, hecho / 1e9, v), end="", flush=True)
                ftp.storbinary("STOR " + parcial, f, blocksize=1 << 20, callback=avance, rest=hecho or None)
            print()
        if tam_remoto(ftp, parcial) != tam:
            raise ftplib.Error("tamano remoto distinto tras subir")
        if tam_remoto(ftp, destino) is not None:
            sys.exit("aparecio %s durante la subida: no se pisa; la nueva quedo como %s" % (destino, parcial))
        ftp.rename(parcial, destino)
        ftp.quit()
        print("listo:", destino)
        break
    except (OSError, EOFError, ftplib.Error) as e:
        print("\n   corte (%s); reintento en 10 s" % e)
        time.sleep(10)
else:
    sys.exit("no se pudo completar la subida")
