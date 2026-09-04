# Empezando

Antes de flashear nada, necesitas tres cosas resueltas:

1. **Hardware correcto** → [Hardware](hardware.md)
2. **Credenciales MQTT rellenadas** → [Configuración (config.h)](config.md)
3. **Saber qué unidad física estás flasheando** → [Identificación de unidades A/B](unidades.md)

Y si vas a flashear una unidad de **pulsadores** en concreto, hay un
paso extra antes de todo esto: decidir qué firmware usar. Ver la
[guía de decisión](../firmware/decision.md).

## Resumen del flujo de trabajo

```mermaid
flowchart TD
    A["`Descargar el proyecto
    (ZIP o git clone)`"] --> B{"¿Qué unidad vas a flashear?"}
    B -- Pulsadores --> C["`Guía de decisión:
    ¿OneButton o AceButton?`"]
    B -- "Dispositivos (relés)" --> D["mega_dispositivos/"]
    C --> E["mega_pulsadores/"]
    C --> F["mega_pulsadores_low_ram/"]
    E --> G["`Rellenar config.h
    de esa carpeta`"]
    F --> G
    D --> G
    G --> H["`Elegir PLACA_A o PLACA_B
    en el .ino`"]
    H --> I["`Editar board_config_X.h
    con los pines reales`"]
    I --> J["Compilar y subir"]
    J --> K["`Verificar en HA:
    Ajustes → MQTT`"]
```
