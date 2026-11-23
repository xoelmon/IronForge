# LARGARTIXA SOUNDLAB — EQ + Compresor + Limitador + medidor LUFS/RMS (VST3)

Cadena fija: **EQ -> Compresor -> Limitador**, y a la derecha un medidor de la salida.

* **EQ:** paso-alto 24 dB/oct + 4 bandas (shelf grave, 2 campanas, shelf agudo). Nodos arrastrables,
  rueda sobre un nodo = Q, doble clic = ganancia a 0. Analizador de espectro.
* **Compresor:** threshold, ratio, knee, attack, release, makeup, mix paralelo.
* **Limitador:** brickwall con lookahead de 5 ms (latencia que el DAW compensa solo), gain, ceiling, release.
* **Medidor de salida (columna derecha):**
  * LUFS (ITU-R BS.1770 / EBU R128): Momentary (400 ms), Short-term (3 s) e Integrated (con puertas -70 LUFS y -10 LU).
  * RMS izquierdo y derecho (ventana de 300 ms, sin ponderar: un seno a -20 dBFS de pico marca -23 dB).
  * Pico de muestra (dBFS). El botón RESET reinicia el Integrated y el pico (úsalo al empezar a reproducir el tema).
  * Mide la señal ya procesada (después del limitador).

## Compilar con GitHub

Sube los archivos de esta carpeta a la raíz de tu repositorio (sustituyen a los anteriores) y GitHub compila solo.
El resultado aparece en Actions -> última ejecución -> Artifacts -> IRONHITS-Windows (el nombre del artefacto no cambia, el contenido sí).
Copia la carpeta `LARGARTIXA SOUNDLAB.vst3` a `C:\Program Files\Common Files\VST3\`.
**Borra de esa carpeta los `IronForge.vst3` e `IRONHITS.vst3` antiguos** para que no salgan los dos.

## Notas
* Sin firma digital: SmartScreen puede avisar al abrir `LARGARTIXA SOUNDLAB.exe`.
* No hay oversampling: no es un limitador true-peak. Para streaming/mp3 pon el ceiling en -1.0 dB.
* Licencia: JUCE es AGPLv3 o de pago; si vendes el plugin sin publicar el código necesitas licencia comercial.
