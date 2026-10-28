# Regenera en docs/efectos.md el contador y la lista de cartas con efecto pendientes, por frecuencia en los mazos
# Uso: python efectos_pendientes.py <remake-ps5> <ia.h> <cartas_classic.txt> <cartas_ui.txt>
import re, sys, collections
raiz, iah, classic, ui = sys.argv[1:5]
cnt = collections.Counter()
for f in ['datos/rivales.txt', 'datos/personajes_rivales.txt']:
    for l in open(raiz + '/' + f, encoding='utf-8'):
        if l[:2] in ('M|', 'X|'):
            for c in l.rstrip('\n').split('|')[3].split():
                cnt[c] += 1
src = open(iah, encoding='utf-8').read()
impl = set(re.findall(r'(?:def|equipo)\("([a-z0-9_]+)"\)', src))
impl |= set(re.findall(r'(?:equipoTipo|sacrificioDanno)\(t, "([a-z0-9_]+)"', src))   # (registradas por funciones auxiliares)
impl |= set(re.findall(r'\{ "([a-z0-9_]+)", \{', src))          # campos de BONOS_CAMPO
for blk in re.findall(r'for \(const char\* id : \{([^}]*)\}\)', src):  # bucles de def(id)
    impl |= set(re.findall(r'"([a-z0-9_]+)"', blk))
sub = {}
for l in open(classic, encoding='utf-8'):
    p = l.rstrip('\n').split('|')
    if len(p) >= 4: sub[p[0]] = p[3]
nombre = {}
for l in open(ui, encoding='utf-8'):
    p = l.rstrip('\n').split('|')
    if len(p) >= 2: nombre[p[0]] = p[1]
# por regla del motor (sin DefCarta propia): Polymerization y las magias de ritual (jugadas J_FUSION/J_RITUAL), los
# Toons (subtipo), los monstruos normales y de fusion sin efecto
regla = {c for c, s in sub.items() if s in ('magia_ritual', 'toon')} | {'polymerization'}
sinEfecto = {c for c, s in sub.items() if s in ('normal', 'fusion')}
total = len(sub)
hechas = len((impl | regla | sinEfecto) & set(sub))
pend = sorted([c for c in sub if c not in impl and c not in regla and c not in sinEfecto], key=lambda c: (-cnt[c], c))
md = raiz + '/docs/efectos.md'
s = open(md, encoding='utf-8').read()
cab = (f'**Cartas con su efecto en el motor: {len(impl & set(sub))}** (DefCarta propia); con las que funcionan por regla '
       f'(Polymerization, magias de ritual, Toons: {len(regla & set(sub))}) y las que no tienen efecto (normales y fusiones sin '
       f'efecto: {len(sinEfecto)}), **{hechas} / {total} cartas jugables con todo lo suyo**. Pendientes: {len(pend)} (lista al '
       f'final, por apariciones en los mazos de rivales.txt y personajes_rivales.txt).')
s = re.sub(r'\*\*Implementadas: [^\n]*', cab, s, count=1)
s = re.sub(r'\*\*Cartas con su efecto en el motor: [^\n]*', cab, s, count=1)
lista = '\n'.join(f'| {cnt[c]} | {c} | {nombre.get(c, "")} | {sub.get(c, "")} |' for c in pend)
sec = ('## Pendientes (por frecuencia en los mazos)\n\nGenerada por efectos_pendientes.py a partir de ia.h, '
       'cartas_classic.txt y los mazos. Se pueden colocar como farol; el menu dice "Efecto aún no disponible".\n\n'
       '| mazos | id | nombre | subtipo |\n|---|---|---|---|\n' + lista + '\n')
if '## Pendientes (por frecuencia en los mazos)' in s:
    s = s[:s.index('## Pendientes (por frecuencia en los mazos)')] + sec
else:
    s = s.rstrip('\n') + '\n\n' + sec
open(md, 'w', encoding='utf-8', newline='\n').write(s)
print(len(impl & set(sub)), hechas, total, len(pend))
