import os,sys,UnityPy
p=sys.argv[1]; res=sys.argv[2]; OUT=sys.argv[3]; os.makedirs(OUT,exist_ok=True)
env=UnityPy.load(p)
R=open(res,'rb')
n=0; fuentes=set()
for o in env.objects:
    if o.type.name!='AudioClip': continue
    d=o.read(); r=d.m_Resource; fuentes.add(r.m_Source)
    R.seek(r.m_Offset); b=R.read(r.m_Size)
    if b[:4]!=b'FSB5': print('no FSB5',d.m_Name,r.m_Source,r.m_Offset); continue
    open(os.path.join(OUT,'builtin__'+d.m_Name+'.fsb'),'wb').write(b); n+=1
print(n,fuentes)
