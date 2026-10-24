#!/usr/bin/env bash
# =============================================================================
# build_flash.sh
# Automatisation de la chaine compilation -> televersement sur STM32
# Projet : Worldwide Weather Watcher (WWW) - carte NUCLEO-L476RG
#
# BUT DU SCRIPT
#   Enchainer en une seule commande les etapes qui se faisaient a la main :
#     1. verifier que les outils sont installes (cmake, arm-none-eabi-gcc)
#     2. incrementer un numero de build (fichier .build_number)
#     3. configurer le projet avec CMake et la toolchain croisee ARM
#     4. compiler le code C avec arm-none-eabi-gcc (compilation croisee :
#        le code est compile sur le PC mais destine au Cortex-M4)
#     5. televerser le fichier .elf sur la carte via le ST-LINK (SWD)
#        avec STM32_Programmer_CLI
#   Si une etape echoue, le script s'arrete (set -e) : on ne televerse jamais
#   un programme qui n'a pas compile.
#
# USAGE
#   ./build_flash.sh [debug|release] [--no-flash] [--clean]
#     debug      : compile en mode Debug (par defaut)
#     release    : compile en mode Release (optimise)
#     --no-flash : compile uniquement, sans televerser (test sans carte)
#     --clean    : supprime le dossier de build avant de compiler
#
# EXEMPLES
#   ./build_flash.sh debug --no-flash   -> verifier que ca compile
#   ./build_flash.sh release            -> compiler en release et televerser
#
# PREREQUIS : Linux (ou WSL / Git Bash), cmake, arm-none-eabi-gcc, ninja ou make,
#             STM32CubeProgrammer, carte branchee en USB (ST-LINK).
# NOTE : script ecrit pour un environnement Linux ; non teste de bout en bout.
# =============================================================================

set -euo pipefail   # arret a la premiere erreur

# ---------- Parametres ----------
MODE="debug"
FLASH=1
CLEAN=0

for arg in "$@"; do
    case "$arg" in
        debug|Debug)       MODE="debug" ;;
        release|Release)   MODE="release" ;;
        --no-flash)        FLASH=0 ;;
        --clean)           CLEAN=1 ;;
        -h|--help)
            sed -n '2,32p' "$0"; exit 0 ;;
        *)
            echo "Argument inconnu : $arg (voir --help)"; exit 1 ;;
    esac
done

# Cmake attend "Debug" / "Release" avec majuscule
if [ "$MODE" = "debug" ]; then BUILD_TYPE="Debug"; else BUILD_TYPE="Release"; fi

# ---------- Chemins ----------
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build/$BUILD_TYPE"
BUILD_NUM_FILE="$PROJECT_DIR/.build_number"

cd "$PROJECT_DIR"

# ---------- Verification des outils ----------
need() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[ERREUR] '$1' est introuvable dans le PATH."
        echo "         $2"
        exit 1
    fi
}

need cmake            "Installe CMake (ou ajoute-le au PATH)."
need arm-none-eabi-gcc "Installe la toolchain ARM GNU (ou ajoute-la au PATH)."

# Generateur : Ninja si dispo, sinon Make
if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
else
    GENERATOR="Unix Makefiles"
fi

# ---------- Numero de build ----------
if [ -f "$BUILD_NUM_FILE" ]; then
    BUILD_NUM=$(( $(cat "$BUILD_NUM_FILE") + 1 ))
else
    BUILD_NUM=1
fi
echo "$BUILD_NUM" > "$BUILD_NUM_FILE"

echo "=============================================="
echo " Build n°$BUILD_NUM  |  mode : $BUILD_TYPE"
echo " Date : $(date '+%Y-%m-%d %H:%M:%S')"
echo "=============================================="

# ---------- Nettoyage optionnel ----------
if [ "$CLEAN" -eq 1 ] && [ -d "$BUILD_DIR" ]; then
    echo "[1/4] Nettoyage de $BUILD_DIR"
    rm -rf "$BUILD_DIR"
else
    echo "[1/4] Pas de nettoyage"
fi

# ---------- Toolchain CubeMX ----------
# Le CMakeLists.txt ne definit pas de compilateur : il faut passer le fichier
# toolchain genere par CubeMX, sinon CMake utiliserait le gcc du PC.
TOOLCHAIN_FILE="$PROJECT_DIR/cmake/gcc-arm-none-eabi.cmake"
if [ ! -f "$TOOLCHAIN_FILE" ]; then
    echo "[ERREUR] Fichier toolchain introuvable : $TOOLCHAIN_FILE"
    echo "         Verifie que le dossier 'cmake' est bien a la racine du projet."
    exit 1
fi

# ---------- Configuration CMake ----------
# Le numero de build est injecte dans le code C sous la forme de la macro
# BUILD_NUMBER (utilisable dans le programme avec printf("%d", BUILD_NUMBER)).
echo "[2/4] Configuration CMake ($GENERATOR)"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
      -G "$GENERATOR" \
      -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_C_FLAGS="-DBUILD_NUMBER=$BUILD_NUM"

# ---------- Compilation ----------
echo "[3/4] Compilation"
if ! cmake --build "$BUILD_DIR" -j; then
    echo "[ERREUR] La compilation a echoue. Aucun televersement."
    exit 1
fi

# Binaire produit par CMake (nom du projet defini dans CMakeLists.txt)
PROJECT_NAME="firste_project"
ELF_FILE="$BUILD_DIR/$PROJECT_NAME.elf"
if [ ! -f "$ELF_FILE" ]; then
    echo "[ERREUR] Fichier $ELF_FILE introuvable apres compilation."
    exit 1
fi
echo "Binaire genere : $ELF_FILE"

# Taille du programme (Flash / RAM)
arm-none-eabi-size "$ELF_FILE" || true

# ---------- Televersement ----------
if [ "$FLASH" -eq 0 ]; then
    echo "[4/4] Televersement ignore (--no-flash)"
    echo "Termine : build n°$BUILD_NUM ($BUILD_TYPE) OK."
    exit 0
fi

# Localisation de STM32_Programmer_CLI
PROG="$(command -v STM32_Programmer_CLI 2>/dev/null || true)"
if [ -z "$PROG" ]; then
    for p in \
        "/c/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe" \
        "/mnt/c/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe" \
        "/opt/st/stm32cubeprogrammer/bin/STM32_Programmer_CLI"
    do
        if [ -x "$p" ]; then PROG="$p"; break; fi
    done
fi

if [ -z "$PROG" ]; then
    echo "[ERREUR] STM32_Programmer_CLI introuvable."
    echo "         Ajoute son dossier 'bin' au PATH ou installe STM32CubeProgrammer."
    exit 1
fi

echo "[4/4] Televersement via ST-LINK (SWD)"
"$PROG" -c port=SWD mode=UR -w "$ELF_FILE" -v -rst

echo "=============================================="
echo " Termine : build n°$BUILD_NUM ($BUILD_TYPE) televerse avec succes."
echo "=============================================="
