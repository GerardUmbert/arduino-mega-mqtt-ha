# Documentación del proyecto (MkDocs)

Fuente de la web de documentación, publicada automáticamente en
GitHub Pages vía [Actions](../.github/workflows/docs.yml) cada vez que
cambia algo en esta carpeta (o `CHANGELOG.md`, que la página de
Changelog transcluye directamente).

## Estructura

```
docs/
├── mkdocs.yml          ← configuración del sitio (nav, tema, plugins)
├── requirements.txt    ← versión pinneada de mkdocs-material
├── docs/                ← contenido real (markdown)
│   ├── index.md
│   ├── getting-started/
│   ├── firmware/
│   ├── blueprints/
│   └── reference/
└── site/                ← salida generada (NO se sube a git, ver .gitignore)
```

## Previsualizar en local

Requiere Python 3.

```bash
pip install -r requirements.txt
mkdocs serve --config-file mkdocs.yml
```

Abre `http://127.0.0.1:8000/` — se recarga sola al guardar cambios.

## Despliegue

No hace falta hacer nada manualmente: cualquier push a `master` que
toque `docs/**` o `CHANGELOG.md` dispara el workflow
[`docs.yml`](../.github/workflows/docs.yml), que construye el sitio
con `mkdocs build --strict` y lo publica en la rama `gh-pages`.

**Primera vez únicamente**: tras el primer despliegue con éxito, hay
que activar GitHub Pages en el repositorio — Settings → Pages → Source
→ elegir la rama `gh-pages` (carpeta `/ (root)`). Es un ajuste de
repositorio, no algo que el workflow pueda hacer por sí solo.

## Añadir una página nueva

1. Crea el fichero `.md` dentro de `docs/` (la carpeta interior).
2. Añádelo a la sección `nav:` de `mkdocs.yml` — si no aparece ahí, no
   sale en el menú aunque el fichero exista.
3. Guarda y haz commit — el despliegue es automático.

## Diagramas Mermaid

Se soportan de forma nativa vía un bloque ` ```mermaid `. No hace
falta ninguna librería extra, ya está configurado en `mkdocs.yml`
(`pymdownx.superfences` con el custom fence `mermaid`).
