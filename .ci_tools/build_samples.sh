# Configurations:
#  temp file name to hold build result
BUILD_RESULT_FILE=build_result.txt

# Repo root directory
REPO_ROOT_DIR=.


declare projects=(
    samples
    apps/OboeTester
)

for d in "${projects[@]}"; do
    pushd ${REPO_ROOT_DIR}/${d} >/dev/null
    TERM=dumb ./gradlew  -q clean bundleDebug
    popd >/dev/null
done


# Check the apks that all get built fine (RhythmGame uses split APKs so we have to specify each one)
declare bundles=(
    samples/hello-oboe/build/outputs/bundle/debug/hello-oboe-debug.aab
    samples/MegaDrone/build/outputs/bundle/debug/MegaDrone-debug.aab
    samples/RhythmGame/build/outputs/bundle/ndkExtractorDebug/RhythmGame-ndkExtractor-debug.aab
    samples/LiveEffect/build/outputs/bundle/debug/LiveEffect-debug.aab
    apps/OboeTester/app/build/outputs/bundle/debug/app-debug.aab
    samples/drumthumper/build/outputs/bundle/debug/drumthumper-debug.aab
)

rm -fr ${BUILD_RESULT_FILE}
for bundle in "${bundles[@]}"; do
  if [ ! -f ${REPO_ROOT_DIR}/${bundle} ]; then
    export SAMPLE_CI_RESULT=1
    echo ${bundle} does not build >> ${BUILD_RESULT_FILE}
  fi
done

if [ -f ${BUILD_RESULT_FILE} ]; then
   echo  "******* Failed Builds ********:"
   cat  ${BUILD_RESULT_FILE}
else
  echo "======= BUILD SUCCESS ======="
fi

rm -fr ${BUILD_RESULT_FILE}
