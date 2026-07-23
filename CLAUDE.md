# Reglas de seguridad para este proyecto

- NUNCA hagas `git add`, `git commit` ni ningún otro comando que incluya
  `config.h` (de `mega_pulsadores/` o `mega_dispositivos/`) en el
  repositorio. Ese fichero contiene la IP real de Home Assistant y las
  credenciales reales del broker MQTT (Mosquitto). Solo `config.h.example`
  (con placeholders) debe subirse.
- Antes de cualquier `git add -A`, `git add .` o commit, revisa
  `git status` y confirma que `config.h` no aparece como fichero a subir.
  Si aparece, algo ha roto el `.gitignore` — párate y avisa, no continúes
  con el commit.
- NUNCA imprimas, muestres ni repitas el contenido de `config.h` (IP,
  usuario o password MQTT) en la conversación, comandos, logs o commits.
- NUNCA hardcodees IP, usuario o password MQTT directamente en los
  ficheros `.ino` — deben vivir siempre en `config.h`, incluido vía
  `#include "config.h"`.
- Si necesitas modificar la plantilla de configuración, edita
  `config.h.example` (con placeholders), nunca copies valores reales de
  `config.h` a un fichero que sí se suba a git.
