# LARGARTIXA SOUNDLAB — cadena de master en un plugin (VST3)

Cadena fija (en este orden):  **EQ -> Compresor -> Clipper -> Limitador 1 -> Limitador 2**,
con un medidor de sonoridad de la salida en la columna derecha.

* **EQ:** paso-alto 24 dB/oct + 4 bandas (shelf grave, 2 campanas, shelf agudo). Nodos arrastrables,
  rueda sobre un nodo = Q, doble clic = ganancia a 0. Analizador de espectro.
  Pestañas 1-4 (o clic en un nodo) para elegir la banda cuyos controles se muestran.
* **EQ dinámico (botón DYN en cada banda):** la banda mueve su ganancia según el nivel de SU propia frecuencia.
  Thresh = nivel a partir del cual actúa, Range = cuántos dB se mueve (negativo = reduce, tipo de-esser o
  anti-resonancia; positivo = realza), Attack y Release = velocidad. Se aplica todo el Range a 12 dB por encima
  del threshold. En la pantalla, el punto naranja bajo el nodo muestra dónde está la banda en cada momento.
  Ejemplo: campana a 6 kHz, Gain 0, Range -6, Thresh ajustado a las eses = de-esser.
* **Compresor:** threshold, ratio, knee, attack, release, makeup, mix paralelo. Filtros HPF/LPF en el sidechain
  (solo afectan a lo que detecta el compresor, no al sonido): con HPF a ~100-150 Hz el bombo no lo hace bombear.
* **Clipper:** drive, ceiling, soft (0 % = recorte duro, 100 % = saturación suave tipo tanh) y mix.
  Funciona con **oversampling x4** para evitar aliasing. Tras el filtro de oversampling la muestra puede pasar
  ~0,3 dB del ceiling: para eso están los limitadores detrás.
* **Limitador 1:** brickwall con lookahead de 5 ms (gain, ceiling, release).
* **Limitador 2:** igual, con **detector true-peak** (interpolación x4 estilo ITU-R BS.1770) activable con el
  botón TRUE PEAK. Con señal tipo música deja el true-peak a ~0,05 dB del ceiling.
* **Medidor de salida:** LUFS (momentary, short-term, integrated) y RMS L/R, pico de muestra. RESET reinicia el
  integrated y el pico. Mide la señal ya procesada (después del limitador 2).
* Cada módulo tiene su botón ON/OFF (LED rojo). Doble clic en un knob = valor por defecto;
  doble clic en el número = escribir un valor.

## Latencia
El plugin declara al DAW la latencia total (oversampling del clipper + lookahead de los dos limitadores):
unas 551 muestras (~11,5 ms a 48 kHz). El DAW la compensa solo al mezclar o masterizar.
Si grabas escuchándote a través del plugin, desactiva los módulos que no necesites o no lo pongas en esa pista.

## Compilar con GitHub
Sube los archivos de esta carpeta a la raíz de tu repositorio (sustituyen a los anteriores) y GitHub compila solo.
Resultado: Actions -> última ejecución -> Artifacts. Copia la carpeta `LARGARTIXA SOUNDLAB.vst3` a
`C:\Program Files\Common Files\VST3\` (con el DAW cerrado, sustituyendo la anterior).

## Notas
* Sin firma digital: SmartScreen puede avisar al abrir el `.exe` independiente.
* Para streaming/mp3: Limitador 2 con TRUE PEAK activado y ceiling en -1.0 dB.
* Licencia: JUCE es AGPLv3 o de pago; si vendes el plugin sin publicar el código necesitas licencia comercial.
