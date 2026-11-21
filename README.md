# TL TRIGGER

Plugin VST3 para Windows que detecta los golpes del audio de entrada y los convierte en disparos de samples que tú cargas (reemplazo/refuerzo de baterías), pensado para FL Studio.

## Cómo funciona

El plugin analiza la señal que entra por la pista del mixer. Cuando un golpe supera el **Threshold**, dispara uno o varios samples con una velocidad calculada según la fuerza del golpe. La señal original se retrasa 4 ms (el plugin lo reporta al host para que FL Studio compense la latencia), así el sample queda alineado con el transitorio original.

| Control | Qué hace |
|---|---|
| THRESHOLD | Nivel a partir del cual se dispara un sample |
| DYNAMICS | 0 % = todos los golpes suenan fuertes; 100 % = la fuerza del golpe controla volumen y capa |
| RETRIG | Tiempo mínimo entre dos disparos (evita dobles disparos) |
| LOW CUT / HIGH CUT | Filtro solo para la detección (no afecta al audio). Útil para aislar bombo o caja |
| TUNE | Afinación de los samples (±12 semitonos) |
| ORIGINAL / SAMPLES | Volumen de la señal original y de los samples (mínimo = silencio) |
| OUTPUT | Volumen de salida |
| INVERT PHASE | Invierte la polaridad de los samples |
| TRIGGER MODE | **Velocity**: los samples cargados se reparten por rangos de fuerza (suave → fuerte). **Round Robin**: van rotando. **Layer**: suenan todos a la vez |

Hasta 8 samples (WAV, AIFF, FLAC, OGG, MP3; máximo 15 s cada uno). Arrastra archivos sobre la lista o usa el botón LOAD. Haz clic en el nombre de un sample para escucharlo.

**Tamaño de la ventana:** arriba a la derecha hay tres botones, 80 %, 100 % (estándar) y 120 % (máximo). La elección se guarda con el proyecto de FL Studio.

## Fuentes

Las fuentes están incrustadas en el plugin (carpeta `Resources/Fonts`):

- **Acidic**: solo "TL TRIGGER", "DRUM TRIGGER" y "SAMPLE REPLACER".
- **Nexa Extra Light** y **Nexa Heavy**: el resto del texto.

Revisa la licencia de estas fuentes antes de hacer público el repositorio o de distribuir el plugin: si no tienes licencia de redistribución, mantén el repositorio en privado.

## Compilar en GitHub (sin instalar nada)

1. Crea un repositorio en GitHub y sube esta carpeta:

```bash
git init
git add .
git commit -m "TL TRIGGER v1.0"
git branch -M main
git remote add origin https://github.com/TU_USUARIO/TL-TRIGGER.git
git push -u origin main
```

2. Entra en la pestaña **Actions** del repositorio: el workflow *Build TL TRIGGER (Windows VST3)* se ejecuta solo (también con **Run workflow**).
3. Al terminar, descarga `TL-TRIGGER-win64-vst3` desde **Artifacts**.
4. Para publicar una versión descargable en **Releases**, crea una etiqueta: `git tag v1.0.0 && git push origin v1.0.0`.

## Instalar en FL Studio

1. Descomprime el zip y copia la carpeta `TL TRIGGER.vst3` en `C:\Program Files\Common Files\VST3`.
2. En FL Studio: *Options → Manage plugins → Find installed plugins* (con *Verify plugins* activado).
3. Carga **TL TRIGGER** como efecto en la pista del mixer donde está el bombo, caja, etc.

Si la previsualización de samples no suena con el transporte parado, revisa en el wrapper del plugin de FL Studio que *Smart disable* no esté apagando el efecto.

## Compilar en local (opcional)

Requisitos: Visual Studio 2022 (carga de trabajo C++) y CMake 3.22+.

```bash
cmake -S . -B build -A x64
cmake --build build --config Release
```

El resultado queda en `build/TLTrigger_artefacts/Release/VST3/TL TRIGGER.vst3`.

## Licencia

Usa el framework [JUCE](https://juce.com), disponible bajo AGPLv3 o licencia comercial. Si publicas el plugin, revisa que tu uso cumpla con la licencia de JUCE.
