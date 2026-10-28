# Yugi Reload - hornea los sistemas del Niagara Examples Pack (Fab) a flipbooks para el motor Vulkan.
#
# Se usan los efectos TAL CUAL (no se replican): Unreal simula cada sistema del pack y una SceneCapture2D
# ortografica lo fotografia cuadro a cuadro. Cada cuadro se toma dos veces en el mismo instante de la
# simulacion: sobre negro y sobre un plano blanco sin luz. De la diferencia sale el alfa exacto
#   negro  = C              (color premultiplicado: lo que el efecto suma)
#   blanco = C + (1 - a) W  (W = el blanco del plano)  ->  a = 1 - (blanco - negro) / W
# asi un efecto aditivo (alfa 0) y uno opaco (humo) salen bien con la misma mezcla premultiplicada del motor.
#
# Corre DENTRO del editor (necesita RHI real para las simulaciones de GPU y el render), un efecto por arranque:
#   UnrealEditor.exe YugiDemo.uproject -ExecCmds="py hornear_niagara.py" con YUGI_FX=<efecto>  (hornear_todo.py lo
#   hace para todos; -ExecutePythonScript no sirve: cierra el editor apenas corre el script)
# Los cuadros crudos (float32) van a Saved/Horneado/crudo/<efecto>; componer_flipbook.py (python del sistema,
# numpy + PIL) arma el atlas y el json, y copia el resultado a assets/fx del juego.
# Variables: YUGI_FX=NS_Fire,NS_Spark_Burst (solo esos), YUGI_FX_RES=512 (lado del cuadro de captura).
import unreal, os, json, array, time, subprocess, math

AQUI = os.path.dirname(os.path.abspath(__file__))
SALIDA = r"D:\Unreal\YugiDemo\Saved\Horneado"
CRUDO = os.path.join(SALIDA, "crudo")
JUEGO = r"D:\Docker\compartido\yugi-src\assets\fx"
PY = os.environ.get("YUGI_PY", "python")   # Python de Windows (no el de Unreal); se fija con YUGI_PY
RES = int(os.environ.get("YUGI_FX_RES", "512"))       # se captura al doble y se reduce (antialias)
LADO = 256                                             # lado de cada cuadro en el atlas
PACK = "/Game/NiagaraExamples/"

# efecto, ruta, tipo (uno = un disparo, loop = se repite), vista (lado: camara horizontal; arriba: cenital),
# cuadros (lado de la grilla), uso en el juego (va al json)
EFECTOS = [
    ("NS_Fire", "FX_Misc/NS_Fire", "loop", "lado", 8, "aliento de fuego (Ojos Rojos y dragones de fuego)"),
    ("NS_Explosion_Medium", "FX_Explosions/NS_Explosion_Medium", "uno", "lado", 8, "impacto del aliento"),
    ("NS_Explosion_Small", "FX_Explosions/NS_Explosion_Small", "uno", "lado", 8, "impacto chico / disparo que pega"),
    ("NS_Explosion", "FX_Explosions/NS_Explosion", "uno", "lado", 8, "explosion grande (maquinas, tanque Oni)"),
    ("NS_ExplosionRoil", "Utilities/SpriteGeneration/ExplosionRoil/NS_ExplosionRoil", "uno", "lado", 8, "bola de fuego"),
    ("NS_Smoke_Plume", "FX_Smoke/NS_Smoke_Plume", "loop", "lado", 8, "humo que queda tras el fuego / maquina rota"),
    ("NS_Spark_Burst", "FX_Sparks/NS_Spark_Burst", "uno", "lado", 8, "chispas y metralla"),
    ("NS_Impact_Metal", "FX_Weapons/Impacts/NS_Impact_Metal", "uno", "lado", 8, "golpe de arma contra metal"),
    ("NS_MuzzleFlash", "FX_Weapons/MuzzleFlashes/NS_MuzzleFlash", "uno", "lado", 6, "fogonazo en la boquilla"),
    ("NS_TeslaCoil", "FX_Ribbons/NS_TeslaCoil", "loop", "lado", 8, "rayos del Mago Oscuro"),
    ("NS_Player_Electricity_Looping", "FX_Player/NS_Player_Electricity_Looping", "loop", "lado", 8, "carga del Mago Oscuro"),
    ("NS_Player_Teleport_In", "FX_Player/NS_Player_Teleport_In", "uno", "lado", 8, "invocacion"),
    ("NS_Player_Teleport_Out", "FX_Player/NS_Player_Teleport_Out", "uno", "lado", 8, "desaparicion del destruido"),
    ("NS_Pickup_Spawn", "FX_PickUp/NS_Pickup_Spawn", "uno", "lado", 8, "aparicion de la carta"),
    ("NS_HitDissolve", "FX_Misc/NS_HitDissolve", "uno", "lado", 8, "golpe que disuelve"),
    ("NS_SkeletalMeshTris_Burst", "FX_Misc/NS_SkeletalMeshTris_Burst", "uno", "lado", 8, "desintegrar en triangulos"),
    ("NS_Dirt_Explosion", "FX_Explosions/NS_Dirt_Explosion", "uno", "lado", 8, "polvo al caer / golpe al suelo"),
    ("NS_Marker_Target", "FX_Markers/NS_Marker_Target", "loop", "arriba", 6, "marca del monstruo objetivo"),
    ("NS_RocketTrail", "FX_Weapons/Trails/NS_RocketTrail", "loop", "lado", 6, "estela del orbe de magia"),
    ("NS_Player_Buff_Looping", "FX_Player/NS_Player_Buff_Looping", "loop", "lado", 8, "aura del atacante"),
    ("NS_Player_DeBuff_Looping", "FX_Player/NS_Player_DeBuff_Looping", "loop", "lado", 8, "aura de magia oscura"),
]
# atlas que el pack ya trae horneados: se exportan tal cual
TEXTURAS = [
    "Utilities/SpriteGeneration/ExplosionRoil/NS_ExplosionRoil_BakedAtlas_Alpha",
    "Utilities/SpriteGeneration/ExplosionRoil/NS_ExplosionRoil_BakedAtlas_Emissive",
    "Utilities/SpriteGeneration/ExplosionRoil/NS_ExplosionRoil_BakedAtlas_Normals",
    "Utilities/SpriteGeneration/ExplosionRoil/NS_ExplosionRoil_BakedAtlas_Occlusion",
    "Utilities/SpriteGeneration/SmokePuffLight/NS_SmokePuffLight_BakedAtlas_Emissive",
    "Utilities/SpriteGeneration/SmokePuffLight/NS_SmokePuffLight_BakedAtlas_Normals",
    "Utilities/SpriteGeneration/SmokePuffLight/NS_SmokePuffLight_BakedAtlas_Occlusion",
    "Utilities/SpriteGeneration/SmokePuffLight/NS_SmokePuffLight_BakedAtlas_Smoke_2048_RGB",
]
# los que nacen de un personaje (muestrean una malla con esqueleto): se les pone el maniqui o el dino de la galeria del
# pack como padre (en la captura no sale: solo el sistema)
MALLAS = {
    "NS_Player_Teleport_In": "/Game/NiagaraExamples/Gallery/SkeletalMesh/Mannequins/Meshes/SKM_Manny_Simple",
    "NS_Player_Teleport_Out": "/Game/NiagaraExamples/Gallery/SkeletalMesh/Mannequins/Meshes/SKM_Manny_Simple",
    "NS_Player_Electricity_Looping": "/Game/NiagaraExamples/Gallery/SkeletalMesh/Mannequins/Meshes/SKM_Manny_Simple",
    "NS_Player_Buff_Looping": "/Game/NiagaraExamples/Gallery/SkeletalMesh/Mannequins/Meshes/SKM_Manny_Simple",
    "NS_Player_DeBuff_Looping": "/Game/NiagaraExamples/Gallery/SkeletalMesh/Mannequins/Meshes/SKM_Manny_Simple",
    "NS_SkeletalMeshTris_Burst": "/Game/NiagaraExamples/Gallery/SkeletalMesh/DinoDragon/SK_DinoDragon",
}
LOOP_PREVIO = 3.0     # segundos de simulacion antes de grabar un loop (que ya este lleno)
LOOP_LARGO = 2.0      # largo del loop grabado (se graba un 25% mas para fundir la cola con el principio)
MAX_UNO = 6.0         # un disparo: se busca su final hasta aca

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rl = unreal.RenderingLibrary


def log(m):
    unreal.log("HORNEAR: " + m)
    with open(os.path.join(SALIDA, "hornear.log"), "a", encoding="utf-8") as f:
        f.write(time.strftime("%H:%M:%S ") + m + "\n")


def material_blanco():
    """Plano de fondo: unlit, emisivo blanco = 1."""
    ruta = "/Game/Horneado/M_FondoBlanco"
    if unreal.EditorAssetLibrary.does_asset_exist(ruta):
        return unreal.EditorAssetLibrary.load_asset(ruta)
    at = unreal.AssetToolsHelpers.get_asset_tools()
    m = at.create_asset("M_FondoBlanco", "/Game/Horneado", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    mel = unreal.MaterialEditingLibrary
    c = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, 0)
    c.set_editor_property("constant", unreal.LinearColor(1, 1, 1, 1))
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_asset(ruta)
    return m


def leer(mundo, rt):
    r = rl.read_render_target_raw(mundo, rt, False)
    px = r[1] if isinstance(r, tuple) else r
    a = array.array("f")
    for c in px:
        a.append(c.r); a.append(c.g); a.append(c.b); a.append(c.a)
    return a


class Estudio:
    """Mundo vacio con el sistema, el plano blanco detras y dos capturas (negro / blanco)."""

    def __init__(self):
        unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        self.mundo = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        sol = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(0, -50, 30))
        sol.light_component.set_editor_property("intensity", 6.0)
        relleno = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(0, 20, 200))
        relleno.light_component.set_editor_property("intensity", 2.0)
        relleno.light_component.set_editor_property("cast_shadows", False)
        self.plano = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        smc = self.plano.static_mesh_component
        smc.set_static_mesh(unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane"))
        smc.set_material(0, material_blanco())
        smc.set_editor_property("cast_shadow", False)
        self.caps = []
        for _ in range(2):
            sc = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
            k = sc.capture_component2d
            k.set_editor_property("capture_every_frame", False)
            k.set_editor_property("capture_on_movement", False)
            k.set_editor_property("projection_type", unreal.CameraProjectionMode.ORTHOGRAPHIC)
            k.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR)
            k.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
            pps = k.post_process_settings
            pps.set_editor_property("override_auto_exposure_method", True)
            pps.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
            pps.set_editor_property("override_auto_exposure_bias", True)
            pps.set_editor_property("auto_exposure_bias", 0.0)
            pps.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
            pps.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
            k.set_editor_property("post_process_settings", pps)
            k.set_editor_property("post_process_blend_weight", 1.0)
            self.caps.append((sc, k))
        self.rts = {}
        self.sistema = None

    def rt(self, n):
        if n not in self.rts:
            self.rts[n] = [rl.create_render_target2d(self.mundo, n, n, unreal.TextureRenderTargetFormat.RTF_RGBA32F,
                                                     unreal.LinearColor(0, 0, 0, 1)) for _ in range(2)]
        return self.rts[n]

    def poner(self, ruta):
        if self.sistema:
            eas.destroy_actor(self.sistema)
        sis = unreal.EditorAssetLibrary.load_asset(PACK + ruta)
        a = eas.spawn_actor_from_class(unreal.NiagaraActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        c = a.niagara_component
        c.set_asset(sis)
        nombre = ruta.split("/")[-1]
        if getattr(self, "cuerpo", None):
            eas.destroy_actor(self.cuerpo)
            self.cuerpo = None
        if nombre in MALLAS:
            # el personaje del que salen las particulas: padre del sistema y tambien pasado por nombre a la interfaz
            self.cuerpo = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
            skc = self.cuerpo.skeletal_mesh_component
            skc.set_skeletal_mesh_asset(unreal.EditorAssetLibrary.load_asset(MALLAS[nombre]))
            a.attach_to_actor(self.cuerpo, "", unreal.AttachmentRule.KEEP_RELATIVE, unreal.AttachmentRule.KEEP_RELATIVE,
                              unreal.AttachmentRule.KEEP_RELATIVE, False)
            for var in ("SkeletalMesh", "User.SkeletalMesh", "SkelMesh", "Mesh", "Character", "SKM"):
                try:
                    unreal.NiagaraFunctionLibrary.override_system_user_variable_skeletal_mesh_component(c, var, skc)
                except Exception:
                    pass
        if os.environ.get("YUGI_FX_TICK") == "1":   # prueba: sin llevar la edad a mano (tiempo real del editor)
            c.set_editor_property("allow_scalability", False)
            c.activate(True)
            self.sistema, self.comp = a, c
            for i, (_, k) in enumerate(self.caps):
                k.clear_show_only_components()
                k.show_only_actor_components(a)
                if i == 1:
                    k.show_only_actor_components(self.plano)
            return
        c.set_age_update_mode(unreal.NiagaraAgeUpdateMode.DESIRED_AGE)
        c.set_seek_delta(1.0 / 60.0)
        c.set_lock_desired_age_delta_time_to_seek_delta(True)
        c.set_max_sim_time(30.0)
        c.set_can_render_while_seeking(True)
        c.set_editor_property("allow_scalability", False)
        c.set_force_solo(True)   # se simula aparte (sin agrupar con otros) para poder llevarle la edad a mano
        # el pack apaga por distancia/visibilidad (la vista del editor no es la captura)
        c.set_desired_age(0.0)
        c.activate(True)
        self.sistema, self.comp = a, c
        for i, (_, k) in enumerate(self.caps):
            k.clear_show_only_components()
            k.show_only_actor_components(a)
            if i == 1:
                k.show_only_actor_components(self.plano)

    def encuadrar(self, vista, centro, ancho, n):
        """Camara ortografica de ancho 'ancho' (cm) mirando a 'centro'; el plano blanco lejos, detras."""
        d = 4000.0
        if vista == "arriba":
            pos, rot = centro + unreal.Vector(0, 0, d), unreal.Rotator(0, -90, 0)
            ppos, prot = centro - unreal.Vector(0, 0, d), unreal.Rotator(0, 0, 0)
        else:   # de lado: la camara en -X mira hacia +X; Y queda horizontal, Z vertical
            pos, rot = centro - unreal.Vector(d, 0, 0), unreal.Rotator(0, 0, 0)
            ppos, prot = centro + unreal.Vector(d, 0, 0), unreal.Rotator(0, 90, 0)   # plano (normal +Z) con pitch 90: mira a -X
        self.plano.set_actor_location_and_rotation(ppos, prot, False, False)
        self.plano.set_actor_scale3d(unreal.Vector(ancho / 20.0, ancho / 20.0, 1))   # el plano basico mide 100 cm
        rts = self.rt(n)
        for (sc, k), rt in zip(self.caps, rts):
            sc.set_actor_location_and_rotation(pos, rot, False, False)
            k.set_editor_property("ortho_width", ancho)
            k.set_editor_property("texture_target", rt)
        # YUGI_FX_VISOR=1: el visor del editor en tiempo real mirando desde la captura (por si una simulacion de GPU
        # necesitara que el visor dibuje)
        if os.environ.get("YUGI_FX_VISOR") == "1":
            try:
                les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
                les.editor_set_viewport_realtime(True)
                if not getattr(self, "ojo", None):   # una camara aparte (pilotar la captura la movia con el visor)
                    self.ojo = eas.spawn_actor_from_class(unreal.CameraActor, pos, rot)
                self.ojo.set_actor_location_and_rotation(pos, rot, False, False)
                les.pilot_level_actor(self.ojo)
            except Exception as e:
                log("visor: %s" % e)
        return rts

    def capturar(self):
        for _, k in self.caps:
            k.capture_scene()


def trabajo():
    os.makedirs(CRUDO, exist_ok=True)
    os.makedirs(JUEGO, exist_ok=True)
    solo = [s for s in os.environ.get("YUGI_FX", "").split(",") if s]
    log("inicio: %d efectos, captura %d px" % (len(EFECTOS), RES))
    for _ in range(60):
        yield     # que el editor termine de abrir
    # en el orden pedido (YUGI_FX) o el de EFECTOS; un mapa vacio nuevo por efecto (en el mismo mapa, despues del
    # primer sistema los siguientes no se dibujaban en la captura) y hasta 2 intentos: si falla se sigue con el resto
    tabla = {e[0]: e for e in EFECTOS}
    orden = [n for n in solo if n in tabla] if solo else [e[0] for e in EFECTOS]
    for nombre in orden:
        _, ruta, tipo, vista, grilla, uso = tabla[nombre]
        salida = os.path.join(SALIDA, nombre + ".json")
        for intento in range(2):
            t0 = time.time()
            antes = os.path.getmtime(salida) if os.path.exists(salida) else 0
            try:
                est = Estudio()
                for _ in range(10):
                    yield
                for _ in hornear(est, nombre, ruta, tipo, vista, grilla, uso):
                    yield
            except Exception as e:
                import traceback
                log("%s FALLA: %s %s" % (nombre, e, traceback.format_exc().replace(chr(10), " | ")))
            ok = os.path.exists(salida) and os.path.getmtime(salida) > antes
            log("%s %s en %.0f s (intento %d)" % (nombre, "LISTO" if ok else "sin resultado", time.time() - t0, intento + 1))
            if ok:
                break
    # atlas que ya vienen horneados en el pack: tal cual (al final: un exportador que no soporta la textura tumba
    # el editor; asi los efectos ya quedaron hechos)
    if not solo or "texturas" in solo:
        dest = os.path.join(CRUDO, "_texturas")
        os.makedirs(dest, exist_ok=True)
        rutas = [PACK + t for t in TEXTURAS]
        unreal.AssetToolsHelpers.get_asset_tools().export_assets(rutas, dest)
        r = subprocess.run([PY, "-I", os.path.join(AQUI, "componer_flipbook.py"), "--texturas", dest,
                            os.path.join(SALIDA, "pack"), os.path.join(JUEGO, "pack")], capture_output=True, text=True)
        log("texturas del pack: %s %s" % (r.stdout.strip(), r.stderr.strip()[-600:]))
    log("fin")


def esperar(est, edad, ticks=2):
    if os.environ.get("YUGI_FX_TICK") == "1":
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_viewport_realtime(True)
        for _ in range(ticks):
            yield
        return
    est.comp.set_desired_age(edad)
    for _ in range(ticks):
        yield


def mascara(neg, bla, n):
    """alfa (lista de n*n) y W (blanco del fondo, de las esquinas)."""
    esq = [0, n - 1, n * (n - 1), n * n - 1]
    w = sorted(bla[4 * i] for i in esq)[1] or 1.0
    al = []
    for i in range(n * n):
        b = 4 * i
        d = ((bla[b] - neg[b]) + (bla[b + 1] - neg[b + 1]) + (bla[b + 2] - neg[b + 2])) / (3 * w)
        lum = (neg[b] + neg[b + 1] + neg[b + 2]) / (3 * w)
        al.append(max(1.0 - d, min(lum, 1.0)))   # lo aditivo cuenta como visible para el encuadre
    return al, w


def recortar(acum, n, cola):
    """Caja (x0, y0, x1, y1) en pixeles que deja afuera a lo sumo 'cola' de la cobertura por cada lado."""
    total = sum(acum)
    if total <= 0:
        return None
    cols = [sum(acum[y * n + x] for y in range(n)) for x in range(n)]
    filas = [sum(acum[y * n:(y + 1) * n]) for y in range(n)]

    def limites(v):
        a, s = 0, 0.0
        while a < n - 1 and s + v[a] < cola * total:
            s += v[a]; a += 1
        b, s = n - 1, 0.0
        while b > 0 and s + v[b] < cola * total:
            s += v[b]; b -= 1
        return a, max(a, b)
    x0, x1 = limites(cols)
    y0, y1 = limites(filas)
    return [x0, y0, x1, y1]


def hornear(est, nombre, ruta, tipo, vista, grilla, uso):
    est.poner(ruta)
    for _ in range(3):
        yield
    # 0) que compile sus materiales: mientras Unreal compila shaders el sistema no se dibuja (se ve vacio). Se le
    # saca foto a lo ancho hasta que aparezca algo (a lo sumo ~2 min)
    rts = est.encuadrar(vista, unreal.Vector(0, 0, 0), 2000.0, 64)
    t0 = time.time()
    k = 0
    while time.time() - t0 < 120:
        edad = (LOOP_PREVIO + 0.4 * (k % 4)) if tipo == "loop" else (0.03, 0.08, 0.2, 0.5)[k % 4]   # rafagas: vida corta
        if k % 4 == 0:
            est.poner(ruta)
            yield
        k += 1
        yield from esperar(est, edad, 3)
        est.capturar()
        neg0, bla0 = leer(est.mundo, rts[0]), leer(est.mundo, rts[1])
        al, _ = mascara(neg0, bla0, 64)
        if os.environ.get("YUGI_FX_DEBUG") == "1" and k % 8 == 1:
            log("  calentando edad %.2f: negro max %.4g, blanco medio %.3g, masa %.3g" % (
                edad, max(neg0[i] for i in range(len(neg0)) if i % 4 != 3), sum(bla0) / len(bla0), sum(a for a in al if a > 0.01)))
        if sum(a for a in al if a > 0.01) > 0.3:
            break
    log("%s: shaders listos en %.0f s" % (nombre, time.time() - t0))
    # 1) sondeo grueso: donde esta y cuanto dura
    n = 96
    ancho = 1200.0
    centro = unreal.Vector(0, 0, 0)
    for intento in range(4):
        rts = est.encuadrar(vista, centro, ancho, n)
        est.poner(ruta)
        yield
        if tipo == "uno":
            tiempos = [0.02 + 0.03 * i for i in range(20)] + [0.65 + 0.1 * i for i in range(int((MAX_UNO - 0.65) / 0.1))]
        else:
            tiempos = [LOOP_PREVIO + 0.1 * i for i in range(int(LOOP_LARGO * 1.25 / 0.1) + 1)]
        acum = [0.0] * (n * n)     # cobertura sumada en el tiempo (para el encuadre)
        masas = []                 # cobertura total en cada instante (para la duracion)
        for t in tiempos:
            yield from esperar(est, t)
            est.capturar()
            neg, bla = leer(est.mundo, rts[0]), leer(est.mundo, rts[1])
            al, w = mascara(neg, bla, n)
            if os.environ.get("YUGI_FX_DEBUG") == "1":
                log("  t=%.2f W=%.4g max negro=%.4g media blanco=%.4g edad=%.3f activo=%s" % (
                    t, w, max(neg), sum(bla) / len(bla), est.comp.get_desired_age(),
                    est.comp.is_active()))
                d = os.path.join(CRUDO, "_sondeo")
                os.makedirs(d, exist_ok=True)
                with open(os.path.join(d, "%s_%05.2f_n.f32" % (nombre, t)), "wb") as f:
                    neg.tofile(f)
                with open(os.path.join(d, "%s_%05.2f_b.f32" % (nombre, t)), "wb") as f:
                    bla.tofile(f)
            m = 0.0
            for i, a in enumerate(al):
                if a > 0.01:
                    acum[i] += a
                    m += a
            masas.append((t, m))
        # caja que guarda el 98 % de todo lo que se vio (las chispas sueltas que caen lejos no agrandan el cuadro)
        caja = recortar(acum, n, 0.01)
        pico = max(m for _, m in masas) if masas else 0.0
        ultimo = max([t for t, m in masas if m > 0.03 * pico] or [0.0])
        if caja is None or pico < 0.25:
            log("%s: no se ve nada (ancho %.0f)" % (nombre, ancho))
            if intento < 2:
                ancho *= 0.25
                continue
            return
        toca = caja[0] <= 1 or caja[1] <= 1 or caja[2] >= n - 2 or caja[3] >= n - 2
        # centro de la caja en mundo (x de la imagen = +Y, y de la imagen = -Z de lado / +X de arriba)
        px = ancho / n
        cx = (0.5 * (caja[0] + caja[2]) + 0.5 - n / 2) * px
        cy = (0.5 * (caja[1] + caja[3]) + 0.5 - n / 2) * px
        lado = max(caja[2] - caja[0], caja[3] - caja[1]) * px
        if vista == "arriba":
            nuevo = centro + unreal.Vector(-cy, cx, 0)
        else:
            nuevo = centro + unreal.Vector(0, cx, -cy)
        if toca and intento < 3:
            centro, ancho = nuevo, ancho * 2.2
            log("%s: se sale del cuadro, se agranda a %.0f" % (nombre, ancho))
            continue
        centro, ancho = nuevo, max(lado * 1.12, 20.0)
        break
    duracion = (ultimo + 0.15) if tipo == "uno" else LOOP_LARGO
    cuadros = grilla * grilla
    log("%s: centro (%.0f, %.0f, %.0f) ancho %.0f cm, dura %.2f s" % (nombre, centro.x, centro.y, centro.z, ancho, duracion))
    # 2) pasada final
    rts = est.encuadrar(vista, centro, ancho, RES)
    est.poner(ruta)
    for _ in range(3):
        yield
    carpeta = os.path.join(CRUDO, nombre)
    os.makedirs(carpeta, exist_ok=True)
    for f in os.listdir(carpeta):
        os.remove(os.path.join(carpeta, f))
    if tipo == "uno":
        tiempos = [duracion * i / cuadros for i in range(cuadros)]
    else:
        extra = int(cuadros * 0.25)
        tiempos = [LOOP_PREVIO + LOOP_LARGO * i / cuadros for i in range(cuadros + extra)]
    for i, t in enumerate(tiempos):
        yield from esperar(est, max(t, 1e-3))
        est.capturar()
        for k, rt in enumerate(rts):
            with open(os.path.join(carpeta, "%03d_%s.f32" % (i, "nb"[k])), "wb") as f:
                leer(est.mundo, rt).tofile(f)
    # origen: donde queda el punto (0,0,0) del sistema dentro del cuadro (0..1, y hacia abajo)
    if vista == "arriba":
        ox, oy = 0.5 + (0 - centro.y) / ancho, 0.5 + (centro.x - 0) / ancho
    else:
        ox, oy = 0.5 + (0 - centro.y) / ancho, 0.5 + (centro.z - 0) / ancho
    meta = {"efecto": nombre, "ruta": PACK + ruta, "tipo": tipo, "vista": vista, "grilla": grilla,
            "cuadros": cuadros, "cuadros_grabados": len(tiempos), "fps": cuadros / duracion, "duracion": duracion,
            "loop": tipo == "loop", "res_captura": RES, "lado": LADO, "tamano_cm": ancho, "origen": [ox, oy],
            "uso": uso}
    with open(os.path.join(carpeta, "meta.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=1)
    r = subprocess.run([PY, "-I", os.path.join(AQUI, "componer_flipbook.py"), carpeta, SALIDA, JUEGO],
                       capture_output=True, text=True)
    log("%s compuesto: %s %s" % (nombre, r.stdout.strip()[-300:], r.stderr.strip()[-600:]))


estado = {"gen": trabajo()}


def tick(dt):
    estado["n"] = estado.get("n", 0) + 1
    if estado.get("ocupado"):     # abrir un mapa o compilar shaders vuelve a pasar por aca: no reentrar
        return
    estado["ocupado"] = True
    try:
        next(estado["gen"])
    except StopIteration:
        log("trabajo terminado (tick %d)" % estado.get("n", 0))
        unreal.unregister_slate_post_tick_callback(estado["h"])
        if os.environ.get("YUGI_FX_NO_SALIR") != "1":
            unreal.SystemLibrary.quit_editor()
    except Exception as e:
        import traceback
        log("ERROR: %s %s" % (e, traceback.format_exc().replace("\n", " | ")))
        unreal.unregister_slate_post_tick_callback(estado["h"])
        unreal.SystemLibrary.quit_editor()
    finally:
        estado["ocupado"] = False


os.makedirs(SALIDA, exist_ok=True)
estado["h"] = unreal.register_slate_post_tick_callback(tick)
