# Reglas de seguridad para este proyecto

- NUNCA menciones ni des crédito a Anthropic, Claude o Claude Code en
  commits, comentarios de código, documentación (README, CHANGELOG,
  context.md, todo.md) ni en ningún otro fichero de este repositorio.
  Ningún trailer tipo "Co-Authored-By" ni referencias de ningún tipo.
- `config.h` (en `mega_pulsadores/` y `mega_dispositivos/`) SÍ está
  trackeado por git — se sube con los `#define` vacíos/placeholder a
  propósito, para que cualquiera que clone el repo lo tenga presente y
  el sketch compile sin pasos extra. Lo que NUNCA debe pasar es que se
  suba una versión de `config.h` con datos reales (IP, usuario o
  password MQTT) rellenados.
- Para evitarlo, ambos `config.h` están marcados localmente con
  `git update-index --skip-worktree`: git ignora cualquier cambio futuro
  en el contenido de esos ficheros aunque los edites y rellenes con tus
  credenciales reales. NUNCA quites ese flag
  (`git update-index --no-skip-worktree config.h`) ni fuerces su
  `git add`/`git commit` con datos reales dentro.
- Antes de cualquier `git add -A`, `git add .` o commit, revisa
  `git status` y confirma que `config.h` no aparece como modificado. Si
  aparece, el flag `skip-worktree` se ha perdido (p. ej. tras un
  `git rm --cached` o clonado nuevo) — párate y avisa, no continúes con
  el commit hasta re-aplicar `skip-worktree` o revertir el contenido a
  placeholders vacíos.
- NUNCA imprimas, muestres ni repitas el contenido real de `config.h`
  (IP, usuario o password MQTT) en la conversación, comandos, logs o
  commits.
- NUNCA hardcodees IP, usuario o password MQTT directamente en los
  ficheros `.ino` — deben vivir siempre en `config.h`, incluido vía
  `#include "config.h"`.
- `config.h.example` es la plantilla documentada (con placeholders
  descriptivos); `config.h` es el fichero real que se rellena y del que
  git ignora los cambios. No confundir ambos ni copiar datos reales de
  uno a otro.
