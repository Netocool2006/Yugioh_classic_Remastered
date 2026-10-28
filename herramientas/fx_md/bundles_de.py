# lista de bundles (rutas) cuyos contenedores empiezan por alguno de los prefijos
import sys,json
d=json.load(open(sys.argv[1])); pre=sys.argv[2:]
s=[]
for b in d:
    if any(k.startswith(p) for k,t,_ in b.get('c',[]) for p in pre): s.append(b['b'])
print('\n'.join(sorted(set(s))))
