#!/bin/sh

###
#  build_dmg.sh
#  affix
#
#  Created by Darryl Ramm on 3/6/24.
####

echo "\$CONFIGURATION = $CONFIGURATION"
echo "\$TARGET_BUILD_DIR = $TARGET_BUILD_DIR"
echo "\$TARGET_NAME = $TARGET_NAME"

if [[ "$CONFIGURATION" == "Release" ]] ; then
    cd "$TARGET_BUILD_DIR"
    
    rm -rf "$TARGET_NAME".dir
    
    /usr/bin/ditto "$TARGET_NAME" "$TARGET_NAME".dir/
    
    hdiutil create -fs HFS+ -format UDZO -nospotlight -srcFolder "$TARGET_NAME".dir "$TARGET_NAME".dmg
    
    xcrun notarytool submit "$TARGET_NAME".dmg --wait --keychain-profile "notarytoolpassword"
    
    if [[ $? == 0 ]] ; then
        xcrun stapler staple -v ./affix.dmg
    else
        echo "notarytool failed not running stapler"
    fi
fi
