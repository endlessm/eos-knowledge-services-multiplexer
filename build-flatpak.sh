#!/bin/bash
set -e
set -x
rm -rf files var metadata export build

BRANCH=${BRANCH:-master}
GIT_CLONE_BRANCH=${GIT_CLONE_BRANCH:-HEAD}
RUN_TESTS=${RUN_TESTS:-false}

build_project() {
    PROJECT=$1

    sed \
      -e "s|@BRANCH@|${BRANCH}|g" \
      -e "s|@GIT_CLONE_BRANCH@|${GIT_CLONE_BRANCH}|g" \
      -e "s|\"@RUN_TESTS@\"|${RUN_TESTS}|g" \
      ${PROJECT}.json.in > ${PROJECT}.json

    flatpak-builder build --force-clean -vv --repo=repo ${PROJECT}.json
    flatpak build-update-repo repo
    flatpak build-bundle repo ${PROJECT}.flatpak ${PROJECT} ${BRANCH}
    flatpak install ${PROJECT}.flatpak
}

build_project com.endlessm.BaseEknServicesMultiplexer
build_project com.endlessm.EknServicesMultiplexer
