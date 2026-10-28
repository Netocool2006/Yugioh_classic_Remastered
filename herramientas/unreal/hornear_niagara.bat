@echo off
rem Hornea los efectos del Niagara Examples Pack (Fab) a flipbooks para el juego (ver hornear_niagara.py).
rem Uso: hornear_niagara.bat              (todos, en una sola sesion del editor)
rem      hornear_niagara.bat NS_Fire NS_Spark_Burst texturas
rem Hace falta GPU real (el editor se abre, trabaja solo y se cierra). Registro: Saved\Horneado\hornear.log
rem Salida: D:\Unreal\YugiDemo\Saved\Horneado (copia de trabajo y vistas previas) y D:\Docker\compartido\yugi-src\assets\fx
python -I "%~dp0hornear_todo.py" %*
