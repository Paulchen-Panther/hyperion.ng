#!/bin/bash -e

error_message() {
  echo "---> Hyperion compilation failed! Abort"; exit 1
}

trap error_message ERR

# set environment variables if not exists
[ -z "${BUILD_TYPE}" ] && BUILD_TYPE="Debug"
[ -z "${TARGET_ARCH}" ] && TARGET_ARCH="linux/amd64"
[ -z "${PLATFORM}" ] && PLATFORM="x11"
[ -z "${OSX_ARCHITECTURE}" ] && OSX_ARCHITECTURE="x86_64"

# Determine cmake build type; tag builds are Release, else Debug (-dev appends to platform)
if [[ $GITHUB_REF == *"refs/tags"* ]]; then
	BUILD_TYPE=Release
else
	PLATFORM=${PLATFORM}-dev
fi

CONFIGURE="cmake -B build -G Ninja -DPLATFORM=${PLATFORM} -DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
BUILD="cmake --build build --target package --config ${BUILD_TYPE}"

if [[ "$RUNNER_OS" == 'macOS' ]]; then
	CORES=$(sysctl -n hw.ncpu)
	CONFIGURE="${CONFIGURE} -DMACOS_ARCHITECTURE=${OSX_ARCHITECTURE} -DCMAKE_INSTALL_PREFIX:PATH=/usr/local"
elif [[ $RUNNER_OS == "Windows" ]]; then
	CORES=$NUMBER_OF_PROCESSORS
	BUILD="${BUILD} --"
elif [[ "$RUNNER_OS" == 'Linux' ]]; then
	CORES=$(nproc)
fi

echo "Compile Hyperion on '${RUNNER_OS}' with build type '${BUILD_TYPE}' and platform '${PLATFORM}'"
echo "Number of Cores $CORES"

if [[ "$RUNNER_OS" == 'macOS' || "$RUNNER_OS" == 'Windows' ]]; then
	mkdir build
	${CONFIGURE}
	${BUILD} -j ${CORES}
elif [[ "$RUNNER_OS" == 'Linux' ]]; then
	echo "Docker arguments used: DOCKER_IMAGE=${DOCKER_IMAGE}, DOCKER_TAG=${DOCKER_TAG}, TARGET_ARCH=${TARGET_ARCH}"
	mkdir ${GITHUB_WORKSPACE}/deploy
	docker run --rm --platform=${TARGET_ARCH} \
		-v "${GITHUB_WORKSPACE}/deploy:/deploy" \
		-v "${GITHUB_WORKSPACE}:/source:rw" \
		-w "/source" \
		ghcr.io/hyperion-project/${DOCKER_IMAGE}:${DOCKER_TAG} \
		/bin/bash -c "mkdir build && ${CONFIGURE} && ${BUILD} -j ${CORES} &&
		cp /source/build/Hyperion-* /deploy/ 2>/dev/null"
fi
