#!/bin/bash

###
# Script inspired by
# https://mokacoding.com/blog/automatic-xcode-versioning-with-git Since we want
# to embed a plist in a command line program we modify the Info.plist in our
# local source not in the built target. We also add version information in both
# the embedded plist and as old SCCS-style @(#) strings. And we add the
# NSHumanReadableCopyright plist value to the version string in both cases.
###

cd "${SRCROOT}"
src_plist="${SRCROOT}/affix/Info.plist"

if [ ! -w "${src_plist}" ]; then
    echo "${0} Error: src_plist=${src_plist} not writable"
    exit 1
fi

echo "${0} src_plist=${src_plist}"

number_of_commits=$(git rev-list HEAD --count)
echo "${0} number_of_commits=${number_of_commits}"
git_release_version=$(git describe --tags --always --abbrev=0)
echo "${0} git_release_version=${git_release_version}"
NSHumanReadableCopyright=$(/usr/libexec/PlistBuddy -c "Print :NSHumanReadableCopyright" "${src_plist}")
echo "${0} NSHumanReadableCopyright=${NSHumanReadableCopyright}"

# Get current versions and copyright string in Info.plist and in version.h
current_number_of_commits=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "${src_plist}")
echo "${0} current_number_of_commits=${current_number_of_commits}"
current_git_release_version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "${src_plist}")
echo "${0} current_git_release_version=${current_git_release_version}"
current_NSHumanReadableCopyright=$(/usr/libexec/PlistBuddy -c "Print :NSHumanReadableCopyright" "${src_plist}")
echo "${0} current_NSHumanReadableCopyright=${current_NSHumanReadableCopyright}"

# If version or build numbers or copyright string has changed we'll rebuild everything, if not just exit

if [[ ${number_of_commits} == ${current_number_of_commits} ]] &&
        [[ ${git_release_version} == ${current_git_release_version} ]] &&
        [[ ${NSHumanReadableCopyright} == ${current_NSHumanReadableCopyright} ]] ; then
        
    # no update needed
    exit 0
    
else

    /usr/libexec/PlistBuddy -c "Set :CFBundleVersion ${number_of_commits}" "${src_plist}"
    echo "${0} Set :CFBundleVersion ${number_of_commits}" "${src_plist}"
    /usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString ${git_release_version}" "${src_plist}"
    echo "${0} Set :CFBundleShortVersionString ${git_release_version#*v}" "${src_plist}"
    sed -i .bak 's|"@(#).*";|"@(#)'"Version ${git_release_version} (${number_of_commits}) ${NSHumanReadableCopyright}"'";|' "${SRCROOT}/affix/version.h"
    fi
