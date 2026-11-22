# IronForge — EQ + Compresor + Limitador (VST3)

Plugin VST3 para FL Studio / Cubase (Windows). Cadena fija: **EQ → Compresor → Limitador**.

* **EQ:** paso-alto 24 dB/oct + 4 bandas (shelf grave, 2 campanas, shelf agudo). Nodos arrastrables en la pantalla,
  rueda del ratón sobre un nodo = Q, doble clic = ganancia a 0. Analizador de espectro de la salida.
* **Compresor:** threshold, ratio, knee, attack, release, makeup, mix (compresión paralela).
* **Limitador:** brickwall con lookahead de 5 ms (latencia que el DAW compensa solo), gain, ceiling, release.
* Cada módulo tiene su botón ON/OFF (el LED rojo). Doble clic en un knob = valor por defecto;
  doble clic en el número de debajo = escribir un valor.

## Opción A (la fácil): que GitHub te lo compile

1. Crea una cuenta gratis en https://github.com y un repositorio nuevo (botón **New**, ponle de nombre `IronForge`).
2. En el repositorio: **Add file → Upload files** y arrastra TODO el contenido de esta carpeta
   (`Source`, `CMakeLists.txt`, `.github`...).
   *Si tu sistema no te deja arrastrar la carpeta oculta `.github`:* en GitHub pulsa **Add file → Create new file**,
   escribe como nombre `.github/workflows/build.yml` y pega dentro el contenido del archivo `build.yml` de este zip.
3. Ve a la pestaña **Actions**. Verás una ejecución "Compilar IronForge (Windows)" en marcha (tarda unos 10 minutos).
   Si no arranca, pulsa en ella → **Run workflow**.
4. Cuando salga el tick verde, entra en la ejecución y abajo, en **Artifacts**, descarga `IronForge-Windows`.
5. Descomprime. Dentro hay:
   * `IronForge.vst3` (es una **carpeta**; cópiala entera) → pégala en `C:\Program Files\Common Files\VST3\`
   * `IronForge.exe` → versión independiente para probarlo sin DAW.
6. **FL Studio:** Options → Manage plugins → *Find installed plugins* (o *Rescan*). Aparecerá en la lista de efectos.
   **Cubase:** Studio → VST Plug-in Manager → Actualizar (o reinicia Cubase). Aparece en Dynamics / EQ.

## Opción B: compilar en tu PC

Necesitas Visual Studio 2022 Community (con la carga de trabajo "Desarrollo para el escritorio con C++"), CMake y Git.

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release --target IronForge_VST3

El plugin queda en `build\IronForge_artefacts\Release\VST3\IronForge.vst3`.

## Notas

* El plugin no está firmado digitalmente: Windows puede avisar con SmartScreen al abrir `IronForge.exe`. Es normal.
* Si algún día el DAW dice que falta `VCRUNTIME140.dll`, instala el "Visual C++ Redistributable" de Microsoft.
* Ceiling del limitador: por defecto -0.3 dB. No hace oversampling, así que no es un limitador de *true-peak*;
  si necesitas margen para mp3/streaming, pon el ceiling en -1.0 dB.
* El ratio, ataque, etc. se pueden automatizar desde el DAW (todos los controles son parámetros).
* Licencia: el código del plugin es tuyo, pero JUCE es AGPLv3 o de pago. Para uso personal da igual;
  si vas a **vender o distribuir sin publicar el código**, necesitas una licencia comercial de JUCE.
