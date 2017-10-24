#!/bin/sh

if [ "$#" -ne 1 ]; then
    echo "Usage: upload_release.sh <release_version>"
    exit 1;
fi

if [ -z ${GITHUB_TOKEN} ]; then
    echo "GITHUB_TOKEN not set. Please obtain a github developer token and set it using: \n\nexport GITHUB_TOKEN=<access token>"
    exit 1;
fi

RELEASE_VERSION=$1

pushd build

GITHUB_RELEASE_BINARY=bin/darwin/amd64/github-release

if [ ! -f ${GITHUB_RELEASE_BINARY} ]; then

    # Download and extract the github release tool
    wget https://github.com/aktau/github-release/releases/download/v0.7.2/darwin-amd64-github-release.tar.bz2 -O darwin-amd64-github-release.tar.bz2
    tar xvjf darwin-amd64-github-release.tar.bz2
fi

if [ ! -f ${GITHUB_RELEASE_BINARY} ]; then
    echo "Github release binary ${GITHUB_RELEASE_BINARY} does not exist."
    exit 1;
fi


function upload_file(){
  echo "Uploading ${1}"
  ${GITHUB_RELEASE_BINARY} upload --user google --repo oboe --tag ${RELEASE_VERSION} \
      --file upload/${1} --name ${1}
}

upload_file cdep-manifest.yml
upload_file oboe-headers.zip
upload_file oboe-armeabi.zip
upload_file oboe-armeabi-v7a.zip
upload_file oboe-arm64-v8a.zip
upload_file oboe-x86.zip
upload_file oboe-x86_64.zip
upload_file oboe-mips.zip

popd