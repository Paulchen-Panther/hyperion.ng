#!/bin/bash

# set environment variables if not exists
[ -z "${BUILD_TYPE}" ] && BUILD_TYPE="Debug"
[ -z "${TARGET_ARCH}" ] && TARGET_ARCH="linux/amd64"
[ -z "${PLATFORM}" ] && PLATFORM="x11"
[ -z "${EXTRA_CMAKE_ARGS}" ] && EXTRA_CMAKE_ARGS=""

# Determine cmake build type; tag builds are Release, else Debug
if [[ $GITHUB_REF == *"refs/tags"* ]]; then
	BUILD_TYPE=Release
fi

echo "Compile Hyperion on '${RUNNER_OS}' with build type '${BUILD_TYPE}' and platform '${PLATFORM}'"

# Build the package on MacOS, Windows or Linux
if [[ "$RUNNER_OS" == 'macOS' ]]; then
	echo "Number of Cores $(sysctl -n hw.ncpu)"
	mkdir build || exit 1
	cmake -B build -G Ninja -DPLATFORM=${PLATFORM} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_INSTALL_PREFIX:PATH=/usr/local ${EXTRA_CMAKE_ARGS} || exit 2
	cmake --build build --target package --parallel $(sysctl -n hw.ncpu) || exit 3
	cd ${GITHUB_WORKSPACE} && source /${GITHUB_WORKSPACE}/test/testrunner.sh || exit 4
	exit 0;
	exit 1 || { echo "---> Hyperion compilation failed! Abort"; exit 5; }
elif [[ $RUNNER_OS == "Windows" ]]; then
	echo "Number of Cores $NUMBER_OF_PROCESSORS"
	mkdir build || exit 1
	cmake -B build -G "Visual Studio 17 2022" -A x64 -DPLATFORM=${PLATFORM} -DCMAKE_BUILD_TYPE="Release" ${EXTRA_CMAKE_ARGS} || exit 2
	cmake --build build --target package --config "Release" -- -nologo -v:m -maxcpucount || exit 3
	exit 0;
	exit 1 || { echo "---> Hyperion compilation failed! Abort"; exit 5; }
elif [[ "$RUNNER_OS" == 'Linux' ]]; then
	echo "Number of Cores $(nproc)"
	echo "Docker arguments used: DOCKER_IMAGE=${DOCKER_IMAGE}, DOCKER_TAG=${DOCKER_TAG}, TARGET_ARCH=${TARGET_ARCH}"
	# verification bypass of external dependencies
	# set GitHub Container Registry url
	REGISTRY_URL="ghcr.io/hyperion-project/${DOCKER_IMAGE}"
	# take ownership of deploy dir
	mkdir ${GITHUB_WORKSPACE}/deploy

	# run docker
	docker run --rm --platform=${TARGET_ARCH} \
		-v "${GITHUB_WORKSPACE}/deploy:/deploy" \
		-v "${GITHUB_WORKSPACE}:/source:rw" \
		-w "/source" \
		$REGISTRY_URL:$DOCKER_TAG \
		/bin/bash -c "mkdir build &&
		cmake -B build -G Ninja -DPLATFORM=${PLATFORM} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ${EXTRA_CMAKE_ARGS} || exit 2 &&
		cmake --build build --target package --parallel $(nproc) || exit 3 &&
		cp /source/build/Hyperion-* /deploy/ 2>/dev/null || : &&
		cd /source && source /source/test/testrunner.sh || exit 5 &&
		exit 0;
		exit 1 " || { echo "---> Hyperion compilation failed! Abort"; exit 5; }

	# overwrite file owner to current user
	sudo chown -fR $(stat -c "%U:%G" ${GITHUB_WORKSPACE}/deploy) ${GITHUB_WORKSPACE}/deploy
fi
