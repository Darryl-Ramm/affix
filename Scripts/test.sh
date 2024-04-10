#!/bin/bash

###
# As a simple test, we use the macOS afinfo program to check the sample 
# rates reported by affix (before and after we change it). 
###

echo "\$CONFIGURATION = $CONFIGURATION"
echo "\$TARGET_BUILD_DIR = $TARGET_BUILD_DIR"

# We use environment variables: BUILT_PRODUCTS_DIR and PROJECT_DIR
TEST_DIR="${TARGET_BUILD_DIR}/aif_test_files"
TEST_FILES_SRC_DIR="${PROJECT_DIR}/aif_test_files"
LOGFILE="${TARGET_BUILD_DIR}/test.log.$(date +%Y-%m-%d-%H-%M-%S)"


cd "${TARGET_BUILD_DIR}"

shopt -s nullglob

function getrate {
   #sets AFINFO_RATE 
   f="$1"

   AFINFO_OUT=$(afinfo -b "${f}" 2>> "${LOGFILE}")
   err=$?
   echo "AFINFO_OUT=${AFINFO_OUT}" >> "${LOGFILE}"

   if [ ${err} -eq 0 ] ; then
      AFINFO_RATE=$(echo "${AFINFO_OUT}" | egrep -o '(\d+) Hz'| cut -f1 -w 2>> "${LOGFILE}")
   else
      echo "Error: afinfo returned ${err}, using affix to get rate:" >> "${LOGFILE}"
      AFINFO_RATE=$(./affix "${f}" 2>> "${LOGFILE}" | rev | cut -f1 | rev )
   fi

   echo "AFINFO_RATE=${AFINFO_RATE}" >> "${LOGFILE}"
}

while getopts p opt; do
   case ${opt} in
      p)
      cp -r "${TEST_FILES_SRC_DIR}" "${TEST_DIR}"
      if [[ $? -ne 0 ]] ; then
         echo "cp -r ${TEST_FILES_SRC_DIR} ${TEST_DIR} failed"
         exit 1
      fi
      ;;
     ?)
      echo "Usage: $(basename "$0") [-p]"
      echo "  p  provision: provision audio test files to build directory, before running tests"
      exit 1
      ;;
   esac
   
done

echo "LOGFILE=${LOGFILE}"

test_rates=(16000 32000 44100 48000 88200 96000 176400 192000)

for file in `find "${TEST_DIR}" \( -iname \*.aif -o -iname \*.aifc -o -iname \*.snd \) -type f -print` ; do

   echo "---------------------"
   echo "---------------------" >> "${LOGFILE}"
   echo "${file}:" >> "${LOGFILE}"
   echo "${file}:"

   getrate "${file}"

   INITIAL_RATE=${AFINFO_RATE}
   echo "INITIAL_RATE=${INITIAL_RATE}"
   echo "INITIAL_RATE=${INITIAL_RATE}" >> "${LOGFILE}"

   for RATE in ${test_rates[@]} ; do

      echo "*" >> "${LOGFILE}"
      echo "Setting RATE=${RATE}" >> "${LOGFILE}"
      NEW_AFFIX_RATE=$(./affix -s ${RATE} "${file}" 2>> "${LOGFILE}"| rev | cut -f1 -w | rev )
      echo "NEW_AFFIX_RATE=${NEW_AFFIX_RATE}" >> "${LOGFILE}"

      AFINFO_OUT=$(afinfo -b "${file}" 2>> "${LOGFILE}")
      err=$?
      echo "${AFINFO_OUT}" >> "${LOGFILE}"

      getrate "${file}"

      AFFIX_RATE=$(./affix "${file}" 2>> "${LOGFILE}" | cut -f2 )
      echo "AFFIX_RATE=${AFFIX_RATE}" >> "${LOGFILE}"

      if [ ${AFINFO_RATE} -eq ${RATE} ] && [ ${NEW_AFFIX_RATE} -eq ${RATE} ] ; then 
         echo "${RATE} : OK" 
         echo "${RATE} : OK" >> "${LOGFILE}"
      else
         echo "${RATE} : FAIL" 
         echo "${RATE} : FAIL" >> "${LOGFILE}"
      fi

   done

   # set file back to start rate
   echo "set file back to start rate:" >> "${LOGFILE}"
   ./affix -s ${INITIAL_RATE} "${file}" >> "${LOGFILE}" 2>&1

done

