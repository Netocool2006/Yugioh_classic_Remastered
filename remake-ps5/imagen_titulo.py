# Imagen de disco del titulo para ShadowMount+: convierte la carpeta compilada (dist/PPSA77002: eboot.bin, sce_sys,
# sce_module, assets...) en UN archivo exFAT (PPSA77002.exfat) sin montar nada ni pedir permisos de administrador.
# ShadowMount+ (v1.7) monta las imagenes .exfat que encuentra en /data/homebrew (y /mnt/usbN/homebrew...) en
# /mnt/shadowmnt/<nombre>_<hash> y registra el titulo como si fuera la carpeta (ver docs/imagen_titulo.md).
#
# Disposicion (la misma que usan las imagenes que ya montan en la consola, perfil "optimized-1.7"):
#   sector 512, cluster 64 KiB (exigido por la via rapida LVD img_type=5 sobre el disco interno), FAT en el
#   sector 128, archivos contiguos en orden de recorrido, cadenas FAT completas, tabla upcase estandar.
#   Fechas fijas: la misma carpeta da siempre la misma imagen byte a byte.
#
# Uso: python -I imagen_titulo.py [carpeta] [--salida X.exfat] [--verificar] [--simular]
#   carpeta   defecto D:\Docker\compartido\ps5\subida\PPSA77002 (o ...\ps5\PPSA77002 si no hay copia de subida)
#   --salida  defecto <carpeta padre de ps5>\PPSA77002.exfat = D:\Docker\compartido\ps5\PPSA77002.exfat
#   --verificar  relee la imagen terminada con un lector exFAT propio y compara cada archivo con la carpeta
#   --simular    solo calcula la disposicion y el tamano
import argparse, array, base64, hashlib, os, struct, sys, time, zlib

SECTOR = 512
CLUSTER = 65536
SPC = CLUSTER // SECTOR
FAT_OFFSET = 128                       # sectores: 2 regiones de arranque (24) + relleno hasta 64 KiB
STAMP = 0x58210000                     # 2024-01-01 00:00:00 (fecha fija = imagen reproducible)
SERIAL = 0x59554749                    # "YUGI"
OMITIR = {"radv-shader-cache", "capturas", "Thumbs.db", "desktop.ini"}

# Tabla upcase comprimida estandar de exFAT (5836 bytes, checksum 0xE619D30D), la que escribe mkfs.exfat
UPCASE_Z = (
    "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"
    "6kGNoGbweVArqB3UCb4I6gb1gvpBg6Bh0ChoHDQJmv6/9s/8PHlz11330D11L91b99F9dT/dXw/QA/UgPVgP0UP1MD1cj9Aj9Sg9"
    "Wo/RY/U4PV5P0BP1JD1ZT9FT9TQ9Xc/QM/UsPVvP0XP1PD1fL9AL9SK9WC/RS/UyvVyv0Cv1Kr1ar9Fr9Tq9Xm/QG/UmvVlv0VF6"
    "q96mt+tovUPv1Lv0br1H79X79H59QB/Uh/RhfUQf1cf0cX1Cn9Sn9Gl9Rp/V5/R5fUFf1Jf0ZX1FX9XX9HX9q76hf9M39e/6lv5D"
    "39Z/6jv6L31X39P39QP9UD/Sj/UT/VQ/08/1C/1Sv9Kv9Rv9Vr/T7zV/+Lj/8HoPY5OGY0AsiAPxIAGEIBEkgWSQAlJBGvhwEKSF"
    "CGSATJAFHGSHnJAb8kA+KACFoAgUgxJQCspAOagAlaAKVIXqUBNqQR2oC/WhITSGptAMWkAraA1toC20hw7QETpBZ+gCXaEbdIee"
    "0Bv6Qn8YCINhKAyHkTAaxsL48ITwxPCk8OTwlPDU8LTw9PCM8EyYDXNhPiyExbAUlsNKWA1rYT1shM0QBdsgGnbCbtgL++EgHIaj"
    "cBxOwmk4C+fhIlyGq3AdbsBNuAW34Q7chfvwEB7DU3gOL+F1+E34bfhd+H2Yw7FMbBPHxDXxTHyTwCQ0/5xTmJQmlUlt0hhtkpnk"
    "/5DDxphMJrPJYrIaZ7KZ7CbHR7mAKWgKmcKmiClqipniH+UKpqKpZCqbKuYzU86U/4dc1VQz1U09U9M0MLVMI1PHNDF1kesjN0Ru"
    "jNzafG3amG9MW9POtDfffpQ3my1mvzlgDppD5qK5ZF6aV+aO+cu8Nm9Md9PDDDSDzGAzxAw1w8xwM+KjPNFMMpPNFDPVTDPTzYyP"
    "8kKzyCw2S8xSs8wsNys+yhvNJrPaRJm1Zp1Zbzb8nT+MKcpsNdvMdhNtdpidZpfZbfaYvWbff451lzlijppj5ry5YE6aU+a0OWPO"
    "mnN/5w/zuGyumKvmmrlt/jQ3zG/mprlrbpk//s4f5nfX3DP3zQPz0Dwyj80T89Q8M8/Ni7/n/2HuT8w7894wfuzxe0OKfAooBsWk"
    "WBSb4lBcikfxKQElpBB9QokoMSWhpJSMklMKSkmpKDWlIY2tb4jIUlpKRxFKTxkoI2WizJSFspKjbJSdclBOykW56VPKQ3kpH+Wn"
    "AlSQClFhKkJFqRgVpxJUkkpRaSpDZakclacKVJEqUWWqQp9RVapG1akG1aTPqRbVpjr0BdWlelSfGlBDakSNqQk1pS+pGTWnFtSS"
    "WtFX1Jq+pjb0DbWldtSevqUO9B11pO+pE/1AnelH6kI/UVf6mbrRL9SdelBP6kW9qQ/1pX7UnwbQQBpEg2kIDaVhNJxG0EgaRaNp"
    "DI2lcTSeJtBEmkSTaQpNpWk0nWbQTJpFs2kOzaV5NJ8W0EJaRItpCS2lZbScVtBKWkWraQ2tpXW0njbQRtpEm2kLRdFW2kbbKZp2"
    "0E7aRbtpD+2lfbSfDtBBOkSH6QgdpWN0nE7QSTpFp+kMnaVzdJ4u0EW6RJfpCl2la3SdfqUb9BvdpN/pFv1Bt+lPukN/0V26R/fp"
    "AT2kR/SYntBTekbP6QW9pFf0mt7QW3pH74lJWM9Kq6xvAxvDxrSxbGwbx8a18Wx8m8AmtCH7iU1kE9skNqlNZpPbFDalTWVT2zRW"
    "27A1lqy1aW06G7HpbQab0WaymW0Wm9U6m81mtzlsTpvL5raf2jw2r81n89sCtqAtZAvbIraoLWaL2xK2pC1lS9sytqwtZ8vbCrai"
    "rWQr2yr2M9RVs9VtDVvTfm5r2dq2jv3C1rX1bH3bwDa0jWxj28Q2tV/aZra5bWFb2lb2K9vafm3b2G9sW9vOtrff/svzPW0v29v2"
    "AebKan1kQ2RjZFNkc2RLJCqyNbItsj0SHdkR2RnZFdkd2RPZG9kX2R85EDkYORQ5HDkSORo5FmEuF1M4Dz/CyvkucDFcTBfLxXZx"
    "XFwXz8V3CVxCF3KfuEQusUvikrpkLrlL4VK6VC61S+M0XpTGkbMurUvnIi69y+Ayukwus8visjrnsrnsrolrCs1cc9fCtXSt3Ffw"
    "NXwD7Vx7963r4L5zHd33rpP7AX50XdxPrqv72XVzv7jurofrCb2hL/SHgTAYhsJwGAmjYSyMh4kwGabCdJgJs2EuzIeFsBiWwnJY"
    "CathLayHjbAZomAbRMNO2A17YT8chMNwFI7DSTgNZ+E8XITLcBWuww246X53t9wf7rb7091xf7m77p677x64h+6Re+yeuKfumXvu"
    "XriX7pV77d64t+6de+/YzQzNCs0OzQnNDc0LzQ8tCC0MLQotDi0JLQ0tCy0PrQitDK0KrQ6tCa0NrQutD20IbQxtCm0ObQlFhbaG"
    "toW2h6JDO0I7Q7tCu0N7QsypT1pOy+k4wuk5A2fkTJyZs3BWdpyNs3MOzsm5ODd/ynk4L+fj/FyAC3IhbsANuRE35ibclL/kZtyc"
    "W3BLbsVfcWv+mtvwN9yW23F7/pY78Hfckb/nTvwDd+YfuQv/xF35Z+7Gv3B37sE9uRf35j7cl/txfx7AA3kQD+YhPJSH8XAewSN5"
    "FI/mMTyWx/F4nsATeRJP5ik8lafxdJ7BM3kWz+Y5PJfn8XxewAt5ES/mJbyUl/FyXsEreRWv5jW8ltfxet7AG3kTb+YtHMVbeRtv"
    "52jewTt5F+/mPbyX9/F+PsAH+RAf5iN8lI/xcT7BJ/kUn+YzfJbP8Xm+wBf5El/mK3yVr/F1/pVv8G98k3/nW/wH3+Y/+Q7/xXf5"
    "Ht/nB/yQH/FjfsJP+Rk/5xf8kl/xa37Db/kdv2fmfwNZziFG"
)


def upcase_tabla():
    t = zlib.decompress(base64.b64decode("".join(UPCASE_Z)))
    assert len(t) == 5836 and suma32(t) == 0xE619D30D
    return t


def suma32(datos):
    s = 0
    for b in datos:
        s = (((s >> 1) | (s << 31)) + b) & 0xFFFFFFFF
    return s


def mapa_upcase(t):
    m = list(range(65536))
    vals = struct.unpack("<%dH" % (len(t) // 2), t)
    i = c = 0
    while i < len(vals) and c < 65536:
        v = vals[i]; i += 1
        if v == 0xFFFF and i < len(vals):
            c += vals[i]; i += 1          # tramo identidad
            continue
        m[c] = v; c += 1
    return m


def div_arriba(v, d):
    return (v + d - 1) // d


class Nodo:
    __slots__ = ("nombre", "ruta", "dir", "tam", "hijos", "c0", "nc")

    def __init__(self, nombre, ruta, es_dir, tam=0):
        self.nombre, self.ruta, self.dir, self.tam = nombre, ruta, es_dir, tam
        self.hijos, self.c0, self.nc = [], 0, 0


def validar_nombre(n):
    if not (1 <= len(n) <= 255) or n in (".", "..") or any(ord(ch) < 32 or ch in '"*/:<>?\\|' for ch in n):
        sys.exit("nombre no representable en exFAT: %r" % n)


def leer_arbol(carpeta):
    raiz = Nodo("", carpeta, True)

    def llenar(nodo):
        for e in os.scandir(nodo.ruta):
            if e.name in OMITIR or e.name.startswith(".") or e.name.endswith(".upload"):
                continue
            validar_nombre(e.name)
            if e.is_dir(follow_symlinks=True):
                h = Nodo(e.name, e.path, True); llenar(h)
            else:
                h = Nodo(e.name, e.path, False, e.stat().st_size)
            nodo.hijos.append(h)
        nodo.hijos.sort(key=lambda x: x.nombre.lower())
        vistos = set()
        for h in nodo.hijos:
            k = h.nombre.upper()
            if k in vistos:
                sys.exit("dos nombres que solo difieren en mayusculas: %s" % os.path.join(nodo.ruta, h.nombre))
            vistos.add(k)
    llenar(raiz)
    return raiz


def recorrer(n):
    yield n
    for h in n.hijos:
        if h.dir:
            yield from recorrer(h)
        else:
            yield h


def entradas_dir(n, es_raiz):
    return (3 if es_raiz else 0) + sum(2 + div_arriba(len(h.nombre), 15) for h in n.hijos)


class Imagen:
    def __init__(self, raiz, upcase):
        self.raiz, self.upcase = raiz, upcase
        self.mapa = mapa_upcase(upcase)
        self.n_up = div_arriba(len(upcase), CLUSTER)
        usados = self.n_up
        for n in recorrer(raiz):
            if n.dir:
                n.nc = max(1, div_arriba(entradas_dir(n, n is raiz) * 32, CLUSTER))
            else:
                n.nc = div_arriba(n.tam, CLUSTER)
            usados += n.nc
        bm = 1
        while True:
            b = div_arriba(div_arriba(usados + bm, 8), CLUSTER)
            if b == bm:
                break
            bm = b
        self.n_bm, self.clusters = bm, usados + bm
        self.fat_sect = div_arriba(div_arriba((self.clusters + 2) * 4, SECTOR), SPC) * SPC
        self.heap_sect = div_arriba(FAT_OFFSET + self.fat_sect, SPC) * SPC
        sig = 2 + bm + self.n_up
        for n in recorrer(raiz):
            if n.nc:
                n.c0, sig = sig, sig + n.nc
        assert sig == 2 + self.clusters
        self.largo = (self.heap_sect + self.clusters * SPC) * SECTOR
        self.archivos = [n for n in recorrer(raiz) if not n.dir]
        self.datos = sum(n.tam for n in self.archivos)

    # ---------------------------------------------------------------------------------------------- piezas
    def arranque(self):
        a = bytearray(12 * SECTOR)
        a[0:3] = b"\xEB\x76\x90"; a[3:11] = b"EXFAT   "
        struct.pack_into("<QQIIIIIIH", a, 64, 0, self.largo // SECTOR, FAT_OFFSET, self.fat_sect,
                         self.heap_sect, self.clusters, self.raiz.c0, SERIAL, 0x0100)
        a[108] = 9; a[109] = SPC.bit_length() - 1; a[110] = 1; a[111] = 0x80; a[112] = 0xFF
        a[510:512] = b"\x55\xAA"
        for i in range(1, 9):
            struct.pack_into("<I", a, (i + 1) * SECTOR - 4, 0xAA550000)
        s = 0
        for j in range(11 * SECTOR):
            if j not in (106, 107, 112):
                s = (((s >> 1) | (s << 31)) + a[j]) & 0xFFFFFFFF
        a[11 * SECTOR:] = struct.pack("<I", s) * (SECTOR // 4)
        return bytes(a)

    def fat(self):
        f = array.array("I", range(1, self.clusters + 3))      # cada cluster apunta al siguiente
        f[0], f[1] = 0xFFFFFFF8, 0xFFFFFFFF
        fines = [2 + self.n_bm - 1, 2 + self.n_bm + self.n_up - 1]
        fines += [n.c0 + n.nc - 1 for n in recorrer(self.raiz) if n.nc]
        for c in fines:
            f[c] = 0xFFFFFFFF
        if sys.byteorder != "little":
            f.byteswap()
        return f.tobytes() + bytes(self.fat_sect * SECTOR - len(f) * 4)

    def bitmap(self):
        b = bytearray(self.n_bm * CLUSTER)
        llenos, resto = divmod(self.clusters, 8)
        b[:llenos] = b"\xFF" * llenos
        if resto:
            b[llenos] = (1 << resto) - 1
        return bytes(b)

    def hash_nombre(self, nombre):
        h = 0
        for ch in nombre:
            u = self.mapa[ord(ch)]
            h = (((h >> 1) | (h << 15)) + (u & 0xFF)) & 0xFFFF
            h = (((h >> 1) | (h << 15)) + (u >> 8)) & 0xFFFF
        return h

    def conjunto(self, n):
        nom = n.nombre.encode("utf-16-le")
        if len(nom) != 2 * len(n.nombre):
            sys.exit("nombre fuera del plano basico de Unicode: %r" % n.nombre)
        ne = div_arriba(len(n.nombre), 15)
        a = bytearray((ne + 2) * 32)
        a[0], a[1] = 0x85, ne + 1
        struct.pack_into("<H2xIII", a, 4, 0x10 if n.dir else 0x20, STAMP, STAMP, STAMP)
        largo = n.nc * CLUSTER if n.dir else n.tam
        a[32] = 0xC0; a[33] = 1 if n.c0 else 0; a[35] = len(n.nombre)
        struct.pack_into("<H", a, 36, self.hash_nombre(n.nombre))
        struct.pack_into("<QIIQ", a, 40, largo, 0, n.c0, largo)
        for j in range(ne):
            o = (2 + j) * 32
            a[o] = 0xC1
            trozo = nom[j * 30:(j + 1) * 30]
            a[o + 2:o + 2 + len(trozo)] = trozo
        s = 0
        for i, b in enumerate(a):
            if i not in (2, 3):
                s = (((s >> 1) | (s << 15)) + b) & 0xFFFF
        struct.pack_into("<H", a, 2, s)
        return bytes(a)

    def directorio(self, n):
        d = bytearray()
        if n is self.raiz:
            d += bytes([0x83]) + bytes(31)                                          # etiqueta vacia
            d += struct.pack("<B19xIQ", 0x81, 2, div_arriba(self.clusters, 8))      # bitmap
            d += struct.pack("<B3xI12xIQ", 0x82, suma32(self.upcase), 2 + self.n_bm, len(self.upcase))
        for h in n.hijos:
            d += self.conjunto(h)
        return bytes(d) + bytes(n.nc * CLUSTER - len(d))

    # ---------------------------------------------------------------------------------------------- escritura
    def escribir(self, salida):
        tmp = salida + ".tmp"
        t0, hecho, ultimo = time.time(), 0, 0
        with open(tmp, "wb", buffering=8 << 20) as o:
            arr = self.arranque()
            o.write(arr); o.write(arr)
            o.write(bytes(FAT_OFFSET * SECTOR - 2 * len(arr)))
            o.write(self.fat())
            o.write(bytes((self.heap_sect - FAT_OFFSET - self.fat_sect) * SECTOR))
            o.write(self.bitmap())
            o.write(self.upcase + bytes(self.n_up * CLUSTER - len(self.upcase)))
            for n in recorrer(self.raiz):
                if n.dir:
                    o.write(self.directorio(n)); continue
                if not n.tam:
                    continue
                with open(n.ruta, "rb") as f:
                    resto = n.tam
                    while resto:
                        b = f.read(min(resto, 8 << 20))
                        if not b:
                            sys.exit("el archivo cambio mientras se leia: " + n.ruta)
                        o.write(b); resto -= len(b); hecho += len(b)
                if os.path.getsize(n.ruta) != n.tam:
                    sys.exit("el archivo cambio mientras se leia: " + n.ruta)
                o.write(bytes(n.nc * CLUSTER - n.tam))
                if hecho - ultimo > 200e6:
                    ultimo = hecho
                    print("\r   %5.1f%%  %.2f GB" % (100 * hecho / max(self.datos, 1), hecho / 1e9), end="", flush=True)
            if o.tell() != self.largo:
                sys.exit("tamano final inesperado: %d != %d" % (o.tell(), self.largo))
        os.replace(tmp, salida)
        print("\r   imagen lista en %.0f s" % (time.time() - t0) + " " * 30)


# -------------------------------------------------------------------------------------------------- lector
def verificar(imagen, carpeta):
    """Lee la imagen como lo haria un driver (arranque, FAT, directorios) y compara cada archivo con la carpeta."""
    with open(imagen, "rb") as f:
        b = f.read(12 * SECTOR)
        assert b[3:11] == b"EXFAT   ", "no es exFAT"
        _, vol, fat_off, fat_len, heap, nclus, raiz_c, _, _ = struct.unpack_from("<QQIIIIIIH", b, 64)
        spc = 1 << b[109]
        cl = SECTOR * spc
        s = 0
        for j in range(11 * SECTOR):
            if j not in (106, 107, 112):
                s = (((s >> 1) | (s << 31)) + b[j]) & 0xFFFFFFFF
        assert struct.unpack_from("<I", b, 11 * SECTOR)[0] == s, "checksum de arranque mal"
        assert vol * SECTOR == os.path.getsize(imagen), "largo de volumen distinto al archivo"
        f.seek(fat_off * SECTOR)
        fat = array.array("I"); fat.frombytes(f.read(fat_len * SECTOR))

        def cadena(c, largo):
            out = []
            while 2 <= c < 0xFFFFFFF7 and len(out) * cl < largo:
                out.append(c); c = fat[c]
            return out

        def leer(c, largo):
            partes = []
            for k in cadena(c, largo):
                f.seek(heap * SECTOR + (k - 2) * cl)
                partes.append(f.read(cl))
            return b"".join(partes)[:largo]

        vistos = 0

        def dir_(c, largo, rel):
            nonlocal vistos
            d = leer(c, largo)
            i = 0
            while i < len(d) and d[i]:
                t = d[i]
                if t == 0x85:
                    sec = d[i + 1]
                    es_dir = struct.unpack_from("<H", d, i + 4)[0] & 0x10
                    ln = d[i + 35]
                    _, c0, dl = struct.unpack_from("<QIIQ", d, i + 40)[1:]
                    nom = b"".join(d[i + 32 * k + 2:i + 32 * k + 32] for k in range(2, sec + 1)).decode("utf-16-le")[:ln]
                    s = 0
                    for j, x in enumerate(d[i:i + 32 * (sec + 1)]):
                        if j not in (2, 3):
                            s = (((s >> 1) | (s << 15)) + x) & 0xFFFF
                    assert s == struct.unpack_from("<H", d, i + 2)[0], "checksum de entrada mal: " + nom
                    r = rel + "/" + nom if rel else nom
                    if es_dir:
                        dir_(c0, dl, r)
                    else:
                        loc = os.path.join(carpeta, *r.split("/"))
                        datos = leer(c0, dl) if dl else b""
                        with open(loc, "rb") as g:
                            if hashlib.sha1(datos).digest() != hashlib.sha1(g.read()).digest():
                                sys.exit("DISTINTO: " + r)
                        vistos += 1
                    i += 32 * (sec + 1)
                else:
                    i += 32
        dir_(raiz_c, 1 << 30, "")
        return vistos


def main():
    ap = argparse.ArgumentParser(description="Carpeta del titulo -> imagen exFAT para ShadowMount+")
    ap.add_argument("carpeta", nargs="?")
    ap.add_argument("--salida")
    ap.add_argument("--verificar", action="store_true")
    ap.add_argument("--simular", action="store_true")
    a = ap.parse_args()
    carpeta = a.carpeta
    if not carpeta:
        carpeta = r"D:\Docker\compartido\ps5\subida\PPSA77002"
        if not os.path.isfile(os.path.join(carpeta, "eboot.bin")):
            carpeta = r"D:\Docker\compartido\ps5\PPSA77002"
    carpeta = os.path.abspath(carpeta)
    titulo = os.path.basename(carpeta)
    for req in ("eboot.bin", os.path.join("sce_sys", "param.json")):
        if not os.path.isfile(os.path.join(carpeta, req)):
            sys.exit("falta %s en %s" % (req, carpeta))
    salida = os.path.abspath(a.salida or os.path.join(r"D:\Docker\compartido\ps5", titulo + ".exfat"))
    print("== leyendo", carpeta)
    img = Imagen(leer_arbol(carpeta), upcase_tabla())
    print("   %d archivos, %.2f GB de datos -> imagen %.2f GB (cluster %d KiB, %d clusters)"
          % (len(img.archivos), img.datos / 1e9, img.largo / 1e9, CLUSTER // 1024, img.clusters))
    if a.simular:
        return
    print("== escribiendo", salida)
    img.escribir(salida)
    if a.verificar:
        print("== verificando")
        n = verificar(salida, carpeta)
        if n != len(img.archivos):
            sys.exit("la imagen tiene %d archivos y la carpeta %d" % (n, len(img.archivos)))
        print("   OK: %d archivos identicos" % n)


if __name__ == "__main__":
    main()
