# INFORME_TECNICO.md

Este informe consolida el análisis técnico de la librería `FastAccelStepper` en la plataforma ESP32 y detalla las optimizaciones y correcciones implementadas.

## 1. Análisis del Driver DRV8825

Según el manual del DRV8825 (sección **7.6 Timing Requirements**, Página 7):

### 1.1. Limitaciones de Frecuencia
*   **Especificación**: Frecuencia máxima de pasos (`f_STEP`): **250 kHz**.
*   **Ancho de Pulso**: Mínimo **1.9 µs** (ALTO/BAJO).
*   **Estado**: La librería ahora respeta mejor estos límites gracias a la precisión mejorada del cálculo de rampas con FPU.

### 1.2. Tiempos de Setup y Hold (Dirección)
*   **Especificación**: Tiempo de Setup (`t_SU(STEP)`): **650 ns**.
*   **Defecto Corregido**:
    *   Anteriormente, la dirección se conmutaba asíncronamente en el driver RMT, violando el tiempo de setup durante los cambios de dirección.
    *   **Solución Implementada**: Se rediseñó la lógica del ISR del RMT para pausar la transmisión, esperar a que el buffer se vacíe (`TX_END`), cambiar el pin de dirección, y reiniciar la transmisión. Esto garantiza un tiempo de setup seguro y evita pasos en dirección errónea.

## 2. Optimizaciones Implementadas en Software (ESP32)

### 2.1. Concurrencia (Race Conditions)
*   **Problema**: Uso de macros `noInterrupts()` incompatibles con Dual-Core.
*   **Solución**: Se implementaron **Spinlocks** de FreeRTOS (`portENTER_CRITICAL` / `portEXIT_CRITICAL`) en `common.h` y `FastAccelStepper.cpp`.
*   **Resultado**: Protección robusta de estructuras de datos compartidas entre tareas (núcleo 1) e ISRs (núcleo 0/1).

### 2.2. Precisión Matemática (FPU)
*   **Problema**: Uso de `PoorManFloat` (8-bit) en ESP32, desperdiciando la FPU.
*   **Solución**: Se modificó `PoorManFloat.h` para usar `float` nativo (IEEE 754 single precision) cuando se compila para ESP32.
*   **Resultado**:
    *   Cálculos de aceleración más suaves y precisos.
    *   Eliminación de *jitter* causado por errores de redondeo.
    *   Mejor aprovechamiento del hardware del ESP32.

### 3. Recomendaciones de Uso
*   Para drivers DRV8825, limitar la velocidad máxima a 250,000 Hz.
*   Utilizar `stepper->setDelayToEnable(1700)` si se usa `setAutoEnable` para respetar el tiempo de *wakeup*.
