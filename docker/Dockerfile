# Everything needed to build the ROM, the converter and the tests, on any
# system that runs Docker: BlocksDS, the Nitro Engine Advanced fork this port
# is written on, the dswifi fork its wireless is, ptexconv, a host compiler
# and CMake.
#
#   docker compose build                       # the default image
#   docker compose build --build-arg WITH_TEAK=1
#       ... with the DSi's DSP toolchain as well (the teak-llvm and libteak
#       forks). That compiles clang: allow an hour and several GB of memory.
#       Without it the ROM builds all the same, with no DSP audio -- which is
#       an experiment, off by default in the game.
#   docker compose build --build-arg WITH_QT=1
#       ... with Qt 6.8 as well, to build the converter's window
#       (hlds-convert-gui) in the container: a check that it builds. The
#       program is linked to the container's Qt; to use it, build it where
#       it is to run (see the README).
#
# The forks are taken at the head of their master branch; the image is built
# again from that step on whenever one of them has moved.

ARG BLOCKSDS_IMAGE=skylyrac/blocksds:slim-latest
ARG WITH_TEAK=0
ARG WITH_QT=0

# ---------------------------------------------------------------- the tools
FROM ${BLOCKSDS_IMAGE} AS base

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential g++ cmake ninja-build git ca-certificates \
        python3 python3-venv python3-pip && \
    rm -rf /var/lib/apt/lists/*

# BlocksDS and its ARM compiler at their newest (the code is C++26: it wants
# the current GCC), and ptexconv, which the converter runs for textures.
# (The upgrade twice: the first time the package manager only brings itself
# up to date. And each step up to five times, a few seconds apart: the name
# servers Docker hands a build on some machines now and then answer "no such
# host" for one that exists.)
RUN again() { n=1; until "$@"; do \
        [ "$n" -ge 5 ] && return 1; n=$((n + 1)); \
        echo "... did not work: again in 5 seconds ($n of 5)"; sleep 5; done; } && \
    again wf-pacman -Syu --noconfirm && \
    again wf-pacman -Syu --noconfirm && \
    again wf-pacman -S --noconfirm --needed blocksds-toolchain blocksds-ptexconv && \
    wf-pacman -Scc --noconfirm && \
    mkdir -p "${BLOCKSDSEXT}"

# ------------------------------------------- the DSP toolchain, if asked for
FROM base AS teak-0

FROM base AS teak-1
ARG TEAK_LLVM_REPO=https://github.com/Warioware64/teak-llvm.git
ARG TEAK_LLVM_REF=master
ARG LIBTEAK_REPO=https://github.com/Warioware64/libteak.git
ARG LIBTEAK_REF=master
# (How many compilers at once: each can take a gigabyte and more.)
ARG TEAK_JOBS=4
ADD https://api.github.com/repos/Warioware64/teak-llvm/git/refs/heads/master /tmp/teak-llvm.ref
RUN git clone --depth 1 --branch "${TEAK_LLVM_REF}" "${TEAK_LLVM_REPO}" /tmp/teak-llvm && \
    cd /tmp/teak-llvm && JOBS="${TEAK_JOBS}" ./build-teak.sh configure build install && \
    rm -rf /tmp/teak-llvm
ADD https://api.github.com/repos/Warioware64/libteak/git/refs/heads/master /tmp/libteak.ref
RUN git clone --depth 1 --branch "${LIBTEAK_REF}" "${LIBTEAK_REPO}" /tmp/libteak && \
    make -C /tmp/libteak install && \
    rm -rf /tmp/libteak

# ---------------------------------------------------- Nitro Engine Advanced
# (With its sound: the game is built with NEA_MAXMOD.)
FROM teak-${WITH_TEAK} AS dev
ARG NEA_REPO=https://github.com/Warioware64/nitro-engine.git
ARG NEA_REF=master
ARG NEA_JOBS=4
ADD https://api.github.com/repos/Warioware64/nitro-engine/git/refs/heads/master /tmp/nea.ref
RUN git clone --depth 1 --branch "${NEA_REF}" "${NEA_REPO}" /tmp/nitro-engine && \
    make -C /tmp/nitro-engine -f Makefile.blocksds -j"${NEA_JOBS}" install NEA_MAXMOD=1 && \
    rm -rf /tmp/nitro-engine

# The wireless: the dswifi fork that also does DS Download Play, installed as
# "dswifi_dl" beside the stock one. (After the engine: nothing of that
# depends on it, and it is the game that links it.)
ARG DSWIFI_REPO=https://github.com/Warioware64/dswifi.git
ARG DSWIFI_REF=master
ADD https://api.github.com/repos/Warioware64/dswifi/git/refs/heads/master /tmp/dswifi.ref
RUN git clone --depth 1 --branch "${DSWIFI_REF}" "${DSWIFI_REPO}" /tmp/dswifi && \
    make -C /tmp/dswifi -j"${NEA_JOBS}" && make -C /tmp/dswifi install && \
    rm -rf /tmp/dswifi

# The ROM's build system is this repository's own copy of architectds (found
# through PYTHONPATH once the repository is mounted); what it needs besides
# is here, where any user of the container can read it.
RUN python3 -m venv /opt/hlds-venv && \
    /opt/hlds-venv/bin/pip install --no-cache-dir ninja pillow
ENV HLDS_VENV=/opt/hlds-venv \
    PYTHONPATH=/work/architectds_nea_mod \
    HOME=/tmp

# (The repository is mounted here, whoever owns it.)
RUN git config --system --add safe.directory /work
WORKDIR /work
CMD ["bash"]

# ------------------------------------- Qt for the converter's window, if asked
# (aqtinstall: the Qt the window needs, 6.8, is newer than many systems'
# own. Widgets and what they stand on only.)
FROM dev AS qt-0

FROM dev AS qt-1
ARG QT_VERSION=6.8.3
USER root
RUN apt-get update && apt-get install -y --no-install-recommends \
        libgl1-mesa-dev libegl1-mesa-dev libxkbcommon-dev libfontconfig1-dev \
        libfreetype6-dev libdbus-1-3 libglib2.0-dev && \
    rm -rf /var/lib/apt/lists/* && \
    python3 -m venv /opt/aqt && \
    /opt/aqt/bin/pip install --no-cache-dir aqtinstall && \
    /opt/aqt/bin/aqt install-qt linux desktop "${QT_VERSION}" linux_gcc_64 -O /opt/qt && \
    rm -rf /opt/aqt
ENV HLDS_QT_DIR=/opt/qt/${QT_VERSION}/gcc_64

FROM qt-${WITH_QT} AS final
