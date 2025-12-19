# Informe de Evaluación Técnica y Optimización: FastAccelStepper en ESP32 con Driver DRV8825

Este informe consolida el análisis técnico de la librería `FastAccelStepper` en la plataforma ESP32, integrando los hallazgos sobre la arquitectura software y las limitaciones específicas del hardware del driver TI DRV8825 (basado en su hoja de datos).

## 1. Análisis del Driver DRV8825 (Documentación Adjunta)

Según el manual del DRV8825 (específicamente la sección **7.6 Timing Requirements**, Página 7), existen restricciones críticas que la librería debe respetar para garantizar un funcionamiento fiable y evitar pérdida de pasos.

### 1.1. Limitaciones de Frecuencia y Ancho de Pulso
*   **Especificación**:
    *   Frecuencia máxima de pasos (`f_STEP`): **250 kHz**.
    *   Duración mínima de pulso en ALTO (`t_WH(STEP)`): **1.9 µs**.
    *   Duración mínima de pulso en BAJO (`t_WL(STEP)`): **1.9 µs**.
*   **Análisis en Librería**:
    *   La implementación RMT (`StepperISR_esp32_rmt.cpp`) genera pulsos con un ciclo de trabajo del 50%.
    *   A la frecuencia máxima del driver (250 kHz), el periodo es de 4.0 µs (2.0 µs ALTO / 2.0 µs BAJO). Esto cumple con el mínimo de 1.9 µs con un margen muy estrecho (100 ns).
    *   **Riesgo**: Si el usuario configura una velocidad superior a 250 kHz (que el ESP32 puede generar fácilmente), el ancho del pulso caerá por debajo de 1.9 µs, violando la especificación del driver y causando que el motor no reconozca los pasos.
*   **Recomendación**: Implementar un límite de software (`MAX_SPEED_HZ`) configurable, predeterminado a 250,000 para este driver, o emitir advertencias si `setSpeedInHz` excede este valor.

### 1.2. Tiempos de Setup y Hold (Dirección)
*   **Especificación**:
    *   Tiempo de Setup (`t_SU(STEP)`): **650 ns** (La dirección debe estar estable 650ns antes del flanco de subida del paso).
*   **Defecto Crítico Detectado en RMT**:
    *   En la implementación actual para ESP32 RMT, el cambio de dirección (`gpio_set_level`) ocurre dentro de la interrupción (`tx_intr_handler`) que recarga el buffer.
    *   Debido a la naturaleza de doble buffer (ping-pong) del RMT, la interrupción se dispara mientras el hardware aún está transmitiendo el bloque anterior de pasos.
    *   **Consecuencia**: El pin `DIR` cambia de estado **mientras se envían los últimos pasos del movimiento anterior**, violando el tiempo de *setup* y potencialmente invirtiendo la dirección de los últimos pasos. Esto es fatal para la precisión posicional.
*   **Recomendación**: Modificar la lógica de cambio de dirección en RMT para asegurar que el buffer anterior se haya vaciado completamente antes de conmutar el pin `DIR`. Esto puede requerir insertar un periodo de "silencio" o espera explícita.

### 1.3. Modos de Energía (Sleep)
*   **Especificación**:
    *   Tiempo de Wakeup (`t_WAKE`): **1.7 ms** (máximo) desde que `nSLEEP` pasa a alto hasta que se aceptan pasos.
*   **Recomendación**: Si se utiliza la gestión automática de energía (`setAutoEnable`), el usuario debe configurar `stepper->setDelayToEnable(1700)` para respetar este tiempo de encendido, ya que el valor por defecto puede ser insuficiente.

## 2. Defectos de Arquitectura Software (ESP32 / FreeRTOS)

### 2.1. Condiciones de Carrera (Race Conditions)
*   **Problema**: Uso de `noInterrupts()` (`portDISABLE_INTERRUPTS`) en un entorno Dual-Core.
*   **Impacto**: Esta macro solo deshabilita interrupciones en el núcleo actual. Si la tarea del stepper corre en el Núcleo 1 y la interrupción ocurre en el Núcleo 0, las variables críticas (`_ro`, `_rw`, colas) pueden corromperse.
*   **Solución**: Implementar **Spinlocks** de FreeRTOS (`portENTER_CRITICAL` / `portEXIT_CRITICAL`) para garantizar exclusión mutua real entre núcleos.

### 2.2. Precisión Matemática (`PoorManFloat`)
*   **Problema**: Uso de una librería propia de punto flotante de 8 bits (`PoorManFloat`) para calcular aceleraciones.
*   **Impacto**: Introduce errores de redondeo y *jitter* (variación de tiempo) en los trenes de pulsos. Innecesario en ESP32, que posee una FPU de hardware (IEEE 754 float) extremadamente rápida.
*   **Solución**: Reemplazar `PoorManFloat` por `float` nativo en la compilación condicional para ESP32. Esto mejorará la suavidad del movimiento y el torque a altas velocidades.

### 2.3. Perfil de Movimiento
*   **Problema**: Perfil de aceleración trapezoidal (aceleración constante).
*   **Impacto**: Cambios bruscos de aceleración (Jerk infinito) que excitan resonancias mecánicas y reducen el torque utilizable.
*   **Solución**: Implementar curvas en **S (S-Curve)** para suavizar el arranque y la parada, permitiendo mayores aceleraciones sin perder pasos.

## 3. Plan de Acción Recomendado

1.  **Refactorización de Concurrencia**:
    *   Sustituir macros de interrupción por bloques críticos (`portENTER_CRITICAL`) en `FastAccelStepper.cpp` y `StepperISR_esp32*.cpp`.
2.  **Corrección del Bug de Dirección (RMT)**:
    *   Rediseñar la máquina de estados en `StepperISR_esp32_rmt.cpp` para sincronizar el cambio de `DIR` con el fin real de la transmisión RMT (posiblemente esperando la interrupción `RMT_CHn_TX_END_INT_ST` antes de cambiar el pin, en lugar de hacerlo en `TX_THR_EVENT`).
3.  **Optimización Matemática**:
    *   Crear una versión de `RampCalculator` que use `float` para ESP32.
4.  **Configuración para DRV8825**:
    *   Documentar o crear un preset que establezca `MAX_SPEED_HZ = 250000` y `DELAY_TO_ENABLE = 1700`.

Este conjunto de mejoras transformará la fiabilidad de la librería para aplicaciones profesionales con ESP32 y drivers industriales como el DRV8825.
