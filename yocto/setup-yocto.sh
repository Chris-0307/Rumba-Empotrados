#!/usr/bin/env bash
#
# setup-yocto.sh - Prepara el entorno Yocto del proyecto RumBa desde cero.
#
# Descarga poky, meta-raspberrypi y meta-openembedded en las versiones
# exactas que usa el curso, y genera build/conf/ a partir de las
# plantillas versionadas en meta-robot/conf/templates/robot/.
#
# Uso:
#   ./yocto/setup-yocto.sh                 # instala en ~/yocto-robot
#   YOCTO_ROOT=/mnt/datos/yocto ./yocto/setup-yocto.sh
#   MACHINE=qemuarm ./yocto/setup-yocto.sh # para probar sin Raspberry
#
# Despues:
#   cd $YOCTO_ROOT && source poky/oe-init-build-env build
#   bitbake robot-image
#
set -euo pipefail

# ---------------------------------------------------------------- config
POKY_TAG="${POKY_TAG:-yocto-5.0.19}"
POKY_BRANCH="scarthgap"
LAYER_BRANCH="scarthgap"

POKY_URL="https://git.yoctoproject.org/poky"
RPI_URL="https://github.com/agherzan/meta-raspberrypi.git"
OE_URL="https://github.com/openembedded/meta-openembedded.git"

YOCTO_ROOT="${YOCTO_ROOT:-$HOME/yocto-robot}"
MACHINE="${MACHINE:-raspberrypi4}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
META_ROBOT="$REPO_ROOT/meta-robot"
TEMPLATES="$META_ROBOT/conf/templates/robot"

say()  { printf '\n\033[1;32m==> %s\033[0m\n' "$*"; }
warn() { printf '\033[1;33m[!] %s\033[0m\n' "$*"; }
die()  { printf '\033[1;31m[x] %s\033[0m\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------- checks
[ -d "$META_ROBOT" ] || die "No encuentro $META_ROBOT. Ejecuta este script desde el repo."

say "Verificando requisitos"

for cmd in git python3 gawk chrpath diffstat zstd lz4 file; do
    command -v "$cmd" >/dev/null 2>&1 || warn "Falta '$cmd' (revisa las dependencias del README)"
done

# Yocto NO construye si /bin/sh es dash.
if [ "$(readlink -f /bin/sh)" = "/usr/bin/dash" ] || [ "$(readlink -f /bin/sh)" = "/bin/dash" ]; then
    warn "/bin/sh apunta a dash. Yocto necesita bash:"
    warn "    sudo dpkg-reconfigure dash     # responder 'No'"
fi

# El espacio en disco es la causa numero uno de builds fallidos.
mkdir -p "$YOCTO_ROOT"
avail_gb=$(df -BG --output=avail "$YOCTO_ROOT" | tail -1 | tr -dc '0-9')
say "Espacio disponible en $YOCTO_ROOT: ${avail_gb} GB"
if [ "$avail_gb" -lt 90 ]; then
    warn "Se recomiendan al menos 90 GB libres. Con menos es probable que"
    warn "el build se detenga por BB_DISKMON_DIRS a mitad de camino."
    read -r -p "    Continuar de todos modos? [s/N] " r
    [ "$r" = "s" ] || [ "$r" = "S" ] || die "Abortado."
fi

# ---------------------------------------------------------------- clonado
clone_or_update() {
    local url="$1" dest="$2" branch="$3" tag="${4:-}"

    if [ -d "$dest/.git" ]; then
        say "Actualizando $(basename "$dest")"
        git -C "$dest" fetch --tags origin "$branch"
    else
        say "Clonando $(basename "$dest") (rama $branch)"
        git clone -b "$branch" "$url" "$dest"
    fi

    if [ -n "$tag" ]; then
        git -C "$dest" checkout -q "$tag"
        say "  $(basename "$dest") fijado en $tag"
    else
        git -C "$dest" checkout -q "$branch"
        git -C "$dest" pull --ff-only -q origin "$branch" || true
        say "  $(basename "$dest") en $branch ($(git -C "$dest" rev-parse --short HEAD))"
    fi
}

clone_or_update "$POKY_URL" "$YOCTO_ROOT/poky"                     "$POKY_BRANCH"  "$POKY_TAG"
clone_or_update "$RPI_URL"  "$YOCTO_ROOT/poky/meta-raspberrypi"    "$LAYER_BRANCH"
clone_or_update "$OE_URL"   "$YOCTO_ROOT/poky/meta-openembedded"   "$LAYER_BRANCH"

# ---------------------------------------------------------------- conf
BUILD_DIR="$YOCTO_ROOT/build"
CONF_DIR="$BUILD_DIR/conf"
mkdir -p "$CONF_DIR"

gen_conf() {
    local sample="$1" target="$2"
    if [ -f "$target" ]; then
        warn "$(basename "$target") ya existe, lo dejo intacto."
        warn "    Borralo si queres regenerarlo desde la plantilla."
        return
    fi
    cp "$sample" "$target"
    say "Generado $target"
}

gen_conf "$TEMPLATES/local.conf.sample"   "$CONF_DIR/local.conf"
gen_conf "$TEMPLATES/bblayers.conf.sample" "$CONF_DIR/bblayers.conf"
cp "$TEMPLATES/conf-notes.txt" "$CONF_DIR/conf-notes.txt"

# Sustituir marcadores por rutas absolutas de esta maquina.
sed -i \
    -e "s|@POKY@|$YOCTO_ROOT/poky|g" \
    -e "s|@METAROBOT@|$META_ROBOT|g" \
    "$CONF_DIR/bblayers.conf"

# DL_DIR y SSTATE_DIR compartidos fuera de build/ para que un
# "rm -rf build/tmp" no obligue a redescargar ni recompilar todo.
if ! grep -q '^DL_DIR' "$CONF_DIR/local.conf"; then
    cat >> "$CONF_DIR/local.conf" <<EOF

# --- rutas generadas por yocto/setup-yocto.sh ---
DL_DIR ?= "$YOCTO_ROOT/downloads"
SSTATE_DIR ?= "$YOCTO_ROOT/sstate-cache"
EOF
fi

# MACHINE solicitada por el usuario.
sed -i "s|^MACHINE ??=.*|MACHINE ??= \"$MACHINE\"|" "$CONF_DIR/local.conf"

mkdir -p "$YOCTO_ROOT/downloads" "$YOCTO_ROOT/sstate-cache"

# ---------------------------------------------------------------- resumen
cat <<EOF

============================================================
  Entorno listo
============================================================
  Raiz Yocto  : $YOCTO_ROOT
  poky        : $POKY_TAG
  Capa propia : $META_ROBOT
  MACHINE     : $MACHINE

  Siguientes pasos:

    cd $YOCTO_ROOT
    source poky/oe-init-build-env build
    bitbake-layers show-layers        # verificar que la capa robot aparece
    bitbake robot-image

  La primera construccion tarda entre 2 y 6 horas.
  Podes dejarla corriendo con:

    bitbake robot-image 2>&1 | tee ~/robot-image-build.log

============================================================
EOF
