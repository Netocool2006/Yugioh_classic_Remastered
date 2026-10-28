# Rama classic-ps5-efectos: previa de una pista de efecto de MD (exportar_pistas.py) vista con la camara de duelo de
# Master Duel (CameraViewSetting "Top": y 95, z -37, FOV 30), para revisar la exportacion sin compilar el juego.
# Uso: python -I previa_pista.py <pista_sin_ext> <salida.png> [cuadros: 0.2,0.4,...] [--op]
import sys, os, struct, math
import numpy as np
from PIL import Image
import moderngl

def leer(base):
    b = open(base + '.mdp', 'rb').read()
    assert b[:4] in (b'MDP1', b'MDP2')
    v2 = b[:4] == b'MDP2'
    nm, nc, fps, dur = struct.unpack_from('<iiff', b, 4); o = 20
    mats = []
    for _ in range(nm):
        x, y, w, h, tile, adit, r, g, bb, a = struct.unpack_from('<ffffiiffff', b, o); o += 40
        borde = struct.unpack_from('<17f', b, o) if v2 else (1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1)
        if v2: o += 68
        mats.append((x, y, w, h, tile, adit, (r, g, bb, a), borde))
    cuadros = []
    for _ in range(nc):
        n, = struct.unpack_from('<i', b, o); o += 4
        cuadros.append(np.frombuffer(b, '<f4', n, o)); o += 4 * n
    return mats, cuadros, fps, dur

def main():
    base, sal = sys.argv[1], sys.argv[2]
    tiempos = [float(x) for x in sys.argv[3].split(',')] if len(sys.argv) > 3 and not sys.argv[3].startswith('--') else [0.1, 0.3, 0.5, 0.8, 1.1, 1.5]
    op = '--op' in sys.argv
    mats, cuadros, fps, dur = leer(base)
    atlas = Image.open(base + '.png').convert('RGBA')
    ctx = moderngl.create_standalone_context()
    tex = ctx.texture(atlas.size, 4, atlas.tobytes()); tex.build_mipmaps()
    prog = ctx.program(vertex_shader='''#version 330
in vec3 p; in vec2 uv; in vec4 c; in vec4 r; in float t;
uniform mat4 VP; out vec2 vuv; out vec4 vc; out vec4 vr; out float vt;
void main(){ gl_Position=VP*vec4(p,1.0); vuv=uv; vc=c; vr=r; vt=t; }''', fragment_shader='''#version 330
in vec2 vuv; in vec4 vc; in vec4 vr; in float vt; uniform sampler2D tex; out vec4 o;
void main(){ int m=int(vt+0.5); vec2 q = (m&1)==1 ? fract(vuv) : clamp(vuv,0.0,1.0); vec4 x=texture(tex, vr.xy+q*vr.zw);
 if((m&2)==2){ float a=smoothstep(1.0-vc.a-0.05,1.0-vc.a+0.05,x.a); o=vec4(x.rgb*vc.rgb*a, 0.0); } else if((m&4)==4) o=vec4(x.rgb*vc.rgb*vc.a, 0.0); else o=vec4(x.rgb*vc.rgb*vc.a, x.a*vc.a); }''')
    W, H = 640, 360
    ojo = np.array([0.0, 95.0, -37.0]);
    if op: ojo = np.array([0.0, 95.0, -37.0])
    f = -ojo / np.linalg.norm(ojo); r = np.cross(np.array([0, 1.0, 0]), f); r /= np.linalg.norm(r); u = np.cross(f, r)
    V = np.eye(4); V[0, :3] = r; V[1, :3] = u; V[2, :3] = f; V[:3, 3] = -V[:3, :3] @ ojo
    fov = math.radians(30); a = W / H; n_, f_ = 1.0, 500.0
    P = np.zeros((4, 4)); P[0, 0] = 1 / (a * math.tan(fov / 2)); P[1, 1] = 1 / math.tan(fov / 2); P[2, 2] = (f_ + n_) / (f_ - n_); P[2, 3] = -2 * f_ * n_ / (f_ - n_); P[3, 2] = 1
    VP = P @ V
    if '--pantalla' in sys.argv:   # (efecto en el espacio de la camara de MD, effectType 26: Black de 952x536 = la pantalla)
        VP = np.eye(4); VP[0, 0] = 1 / 4.7636; VP[1, 1] = 1 / 2.6795; VP[2, 2] = 0.001
    hoja = Image.new('RGB', (W * 3, H * ((len(tiempos) + 2) // 3)), (20, 20, 28))
    for k, tt in enumerate(tiempos):
        c = cuadros[min(len(cuadros) - 1, int(round(tt * fps)))]
        verts = []
        i = 0
        while i < len(c):
            tipo = int(c[i]); m = mats[int(c[i + 1])]
            rr = (m[0], m[1], m[2], m[3]); tl = float(m[4] | (4 if m[5] else 0)); tin = np.array(m[6])
            def v(pp, uv, col): verts.append([*pp, *uv, *(np.array(col) * tin), *rr, tl])
            if tipo == 0:
                p = c[i + 2:i + 5]; sx, sy = c[i + 5] / 2, c[i + 6] / 2; rot = c[i + 7]; col = c[i + 8:i + 12]; ua, va, ub, vb = c[i + 12:i + 16]
                ca, sa = math.cos(-rot), math.sin(-rot); A = r * ca + u * sa; B = -r * sa + u * ca
                Q = [p - A * sx + B * sy, p + A * sx + B * sy, p + A * sx - B * sy, p - A * sx - B * sy]; U = [(ua, va), (ub, va), (ub, vb), (ua, vb)]
                for j in (0, 1, 2, 0, 2, 3): v(Q[j], U[j], col)
                i += 16
            elif tipo == 1:
                p = c[i + 2:i + 5]; L = c[i + 5:i + 8]; hw = c[i + 8]; col = c[i + 9:i + 13]; ua, va, ub, vb = c[i + 13:i + 17]
                d = L / max(np.linalg.norm(L), 1e-6); A = np.cross(d, f); A = A / max(np.linalg.norm(A), 1e-6) * hw
                Q = [p - A - L, p + A - L, p + A, p - A]; U = [(ua, va), (ub, va), (ub, vb), (ua, vb)]
                for j in (0, 1, 2, 0, 2, 3): v(Q[j], U[j], col)
                i += 17
            elif tipo == 3:   # con normales: la luz de borde de MD por vertice
                col = np.array(c[i + 26:i + 30]); bd = m[7]
                cull = int(os.environ.get('CULL', '0'))
                if cull and not m[5]:   # (cara de atras: fuera, como Cull Back de Unity; se prueba el signo en pantalla y hacia abajo)
                    P3 = [VP @ np.array([*c[i + 2 + 8 * j:i + 5 + 8 * j], 1.0]) for j in range(3)]
                    S = [np.array([p_[0] / p_[3], -p_[1] / p_[3]]) for p_ in P3]
                    cr = (S[1][0] - S[0][0]) * (S[2][1] - S[0][1]) - (S[1][1] - S[0][1]) * (S[2][0] - S[0][0])
                    if cr * cull > 0: i += 30; continue
                for j in range(3):
                    q = c[i + 2 + 8 * j:i + 10 + 8 * j]; pp = np.array(q[:3]); nn = np.array(q[5:8])
                    vv = (np.array([0, 0, -1.0]) if '--pantalla' in sys.argv else (ojo - pp) / max(np.linalg.norm(ojo - pp), 1e-6))
                    x = 1.0 - abs(float(nn @ vv))
                    fr = 0.0 if bd[7] < 0.5 else (x ** bd[6]) if bd[7] < 1.5 else float(np.clip((x - bd[8]) / max(bd[9] - bd[8], 1e-4), 0, 1))
                    lz = max(float(nn @ np.array(bd[13:16])), 0.0) ** bd[16]
                    cv = np.concatenate([col[:3] * (np.array(bd[:3]) + np.array(bd[10:13]) * lz) + np.array(bd[3:6]) * fr / np.maximum(tin[:3], 1e-6), col[3:]])
                    v(pp, q[3:5], cv)
                i += 30
            else:
                col = c[i + 17:i + 21]
                for j in range(3): v(c[i + 2 + 5 * j:i + 5 + 5 * j], c[i + 5 + 5 * j:i + 7 + 5 * j], col)
                i += 21
        fbo = ctx.simple_framebuffer((W, H)); fbo.use(); fbo.clear(0.08, 0.08, 0.11, 1)
        if verts:
            ctx.enable(moderngl.BLEND); ctx.blend_func = (moderngl.ONE, moderngl.ONE_MINUS_SRC_ALPHA)
            prog['VP'].write(VP.T.astype('f4').tobytes()); tex.use(0)
            vbo = ctx.buffer(np.array(verts, 'f4').tobytes())
            vao = ctx.vertex_array(prog, [(vbo, '3f 2f 4f 4f 1f', 'p', 'uv', 'c', 'r', 't')]); vao.render(); vao.release(); vbo.release()
        img = Image.frombytes('RGB', (W, H), fbo.read(components=3)).transpose(Image.FLIP_TOP_BOTTOM)
        hoja.paste(img, ((k % 3) * W, (k // 3) * H))
    hoja.save(sal)

if __name__ == '__main__':
    main()
