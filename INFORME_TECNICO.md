# Informe de Evaluación Técnica: Librería FastAccelStepper en ESP32

Este informe detalla la evaluación del funcionamiento de la librería `FastAccelStepper` en la plataforma ESP32, enfocándose en la generación de rampas, lógica de control, torque, tiempos de respuesta y manejo de concurrencia (FreeRTOS).

## 1. Defectos Críticos de Concurrencia y Threading (FreeRTOS)

### Problema: Race Conditions en Sistemas Dual-Core
La librería utiliza las macros `fasDisableInterrupts()` y `fasEnableInterrupts()` para proteger secciones críticas. En ESP32, estas se definen como `portDISABLE_INTERRUPTS` y `portENABLE_INTERRUPTS` (en `common.h`).

*   **Defecto**: En el ESP32 (que posee dos núcleos), `portDISABLE_INTERRUPTS` solo deshabilita las interrupciones **en el núcleo actual**. No impide que una interrupción o una tarea en el **otro núcleo** acceda a los mismos datos compartidos.
*   **Impacto**: Si la tarea de control (`StepperTask`) se ejecuta en el Núcleo 1 y la interrupción (ISR) del motor se ejecuta en el Núcleo 0 (o viceversa), existe una condición de carrera (race condition).
    *   Variables críticas como `read_idx`, `next_write_idx` en `StepperQueue` podrían corromperse si no se garantiza la atomicidad o el orden de memoria correcto.
    *   Estructuras compartidas como `_ro` y `_rw` en `RampGenerator` son copiadas byte a byte. Si una interrupción o tarea interrumpe esta copia desde otro núcleo, se pueden leer datos inconsistentes (mezcla de datos viejos y nuevos).
*   **Corrección Recomendada**:
    *   Utilizar **Spinlocks** (`portENTER_CRITICAL` / `portEXIT_CRITICAL`) para proteger el acceso a variables compartidas entre ISR y Tareas en un entorno multicore. Esto asegura que el otro núcleo espere antes de acceder a la sección crítica.

### Problema: Prioridad de Tarea Excesiva
*   **Observación**: `StepperTask` se crea con prioridad `configMAX_PRIORITIES`.
*   **Impacto**: Esto puede causar inanición (starvation) de otras tareas críticas del sistema (como WiFi o Bluetooth) si la gestión de steppers toma demasiado tiempo.
*   **Mejora**: Evaluar si una prioridad tan alta es estrictamente necesaria o si se puede reducir ligeramente, confiando en el buffer de la cola (`QUEUE_LEN`) para absorber latencias.

## 2. Precisión Matemática y Lógica de Rampas

### Problema: Uso de `PoorManFloat` en ESP32
La librería utiliza una implementación propia de punto flotante de 8-bit (`PoorManFloat`) diseñada para microcontroladores AVR de 8 bits sin FPU (Unidad de Punto Flotante).

*   **Defecto**: El ESP32 cuenta con una **FPU de precisión simple (float)** por hardware que es extremadamente rápida.
*   **Impacto**:
    *   **Pérdida de Precisión**: `PoorManFloat` tiene una mantisa de solo 8 bits (~2.4 dígitos decimales). Esto introduce errores de redondeo significativos en los cálculos de aceleración y velocidad, lo que puede causar **jitter** (variación en el tiempo entre pasos). El jitter reduce el torque efectivo y aumenta la vibración.
    *   **Ineficiencia**: Emular punto flotante por software (con tablas de búsqueda y desplazamientos) en un chip con FPU hardware es innecesario.
*   **Corrección Recomendada**:
    *   Reemplazar `PoorManFloat` por `float` nativo (IEEE 754) en la implementación para ESP32. Esto mejorará drásticamente la precisión del cálculo de tiempos (`ticks`) y la suavidad del movimiento.

### Problema: Perfil de Aceleración Limitado (Solo Trapezoidal)
La librería implementa únicamente rampas de aceleración constante (perfil trapezoidal de velocidad).

*   **Defecto**: En un perfil trapezoidal, el cambio de aceleración es instantáneo (Jerk infinito) al inicio y al final de la rampa.
*   **Impacto en Torque**:
    *   Los cambios bruscos de aceleración inducen vibraciones mecánicas que pueden superar el torque de retención del motor, causando **pérdida de pasos** o estancamiento (stall), especialmente a altas velocidades o con cargas inerciales.
*   **Mejora**:
    *   Implementar curvas de aceleración en **S (S-Curve)**. Esto limita el "Jerk" (la derivada de la aceleración), suavizando las transiciones. Esto permite alcanzar mayores velocidades y aceleraciones sin perder torque, ya que se evita excitar las frecuencias de resonancia del sistema mecánico.

## 3. Respuesta y Gestión de Colas

### Latencia de Respuesta
*   **Observación**: La cola de comandos tiene una longitud fija (`QUEUE_LEN = 32`).
*   **Impacto**: Cualquier cambio en la velocidad o posición destino se agrega al final de la cola. El motor debe ejecutar todos los comandos previos antes de reaccionar al cambio.
*   **Mejora**: Aunque existe `forceStop()` para paradas de emergencia, para cambios dinámicos de velocidad se podría implementar una función que modifique los comandos existentes en la cola o permita una "limpieza segura" parcial para una respuesta más ágil.

### Riesgo de "Queue Starvation" a Altas Velocidades
*   **Observación**: A muy altas velocidades, cada comando en la cola representa un tiempo muy corto. Si la tarea `manageSteppers` (que corre cada 4ms) no rellena la cola lo suficientemente rápido, esta se vacía.
*   **Lógica Actual**: La librería intenta agrupar múltiples pasos en un solo comando (`planning_steps`) para mitigar esto.
*   **Riesgo**: Si la lógica de agrupación falla o el sistema está muy cargado, el motor puede tartamudear.
*   **Mejora**: Aumentar el tamaño de la cola en ESP32 (donde la RAM es abundante) de 32 a 64 o 128 entradas proporcionaría un buffer de seguridad mayor.

## 4. Implementación Hardware (Driver ESP32)

### Complejidad en MCPWM/PCNT
*   **Observación**: La implementación `StepperISR_esp32_mcpwm_pcnt.cpp` es ingeniosa pero compleja. Utiliza el PCNT (contador de pulsos) para contar los pasos generados por el PWM y disparar una interrupción.
*   **Defecto Potencial**: Hay lógica compleja para manejar casos donde la interrupción llega "tarde" (`if (PCNT.conf_unit[pcnt_unit].conf2.cnt_h_lim != steps)`). Esto sugiere que el sistema está operando cerca de sus límites temporales.
*   **Mejora**: Simplificar la lógica o confiar más en el hardware RMT (Remote Control Peripheral) que suele ser más robusto para generación de trenes de pulsos precisos sin tanta intervención de la CPU.

## Resumen de Recomendaciones

1.  **Seguridad en Hilos**: Reemplazar `fasDisableInterrupts` con `portENTER_CRITICAL` en las secciones críticas de ESP32 para evitar corrupción de memoria entre núcleos.
2.  **Precisión**: Eliminar `PoorManFloat` y usar `float` estándar para aprovechar la FPU del ESP32 y eliminar el jitter de cálculo.
3.  **Torque y Suavidad**: Implementar rampas **S-Curve** para reducir vibraciones y maximizar el torque útil.
4.  **Buffer**: Aumentar `QUEUE_LEN` en ESP32 para mayor robustez a altas velocidades.
