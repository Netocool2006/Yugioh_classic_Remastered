# Yugi Reload - hornea los efectos del Niagara Examples Pack en UNA sesion del editor de Unreal (los shaders se
# compilan una vez; cada efecto en un mapa vacio nuevo y con 2 intentos: si uno falla se sigue con el resto).
# Uso: python -I hornear_todo.py [NS_Fire NS_Spark_Burst ... texturas]   (sin argumentos: todos + texturas)
# Registro: D:\Unreal\YugiDemo\Saved\Horneado\hornear.log
import os, re, subprocess, sys, time

AQUI = os.path.dirname(os.path.abspath(__file__))
UE = r"D:\Unreal\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
PROYECTO = r"D:\Unreal\YugiDemo\YugiDemo.uproject"
SCRIPT = os.path.join(AQUI, "hornear_niagara.py").replace("\\", "/")

texto = open(os.path.join(AQUI, "hornear_niagara.py"), encoding="utf-8").read()
todos = re.findall(r'^\s+\("(NS_\w+)", "', texto, re.M)
pedidos = sys.argv[1:] or todos + ["texturas"]
t0 = time.time()
env = dict(os.environ, YUGI_FX=",".join(pedidos))
try:
    subprocess.run([UE, PROYECTO, "-ExecCmds=py %s" % SCRIPT, "-unattended", "-nosplash", "-nop4", "-NoSound",
                    "-dpcvars=r.WarnOfBadDrivers=0"], env=env, timeout=3 * 3600)
except subprocess.TimeoutExpired:
    subprocess.run(["taskkill", "/F", "/IM", "UnrealEditor.exe"], capture_output=True)
print("%d pedidos en %.0f min" % (len(pedidos), (time.time() - t0) / 60))
