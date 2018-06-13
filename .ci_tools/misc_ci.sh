#!/bin/bash
set +e

MISC_STATUS=0

# check that all targetSdkVersion are 26+
# test "$(grep -H targetSdkVersion */app/build.gradle | tee /dev/stderr | cut -d= -f 2 | xargs -n1 echo | sort | uniq | wc -l)" = "2"
# check that there is no tabs in AndroidManifest
(! grep -n $'\t' */*/src/main/AndroidManifest.xml) | cat -t;
MISC_STATUS=$(($MISC_STATUS + ${PIPESTATUS[0]}))

# check that there is no trailing spaces in AndroidManifest
(! grep -E '\s+$' */*/src/main/AndroidManifest.xml) | cat -e;
MISC_STATUS=$(($MISC_STATUS + ${PIPESTATUS[0]}))

# populate the error to final status
if [[ "$MISC_STATUS" -ne 0 ]]; then
    SAMPLE_CI_RESULT=$(($SAMPLE_CI_RESULT + 1))
fi
