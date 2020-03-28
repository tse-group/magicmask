#!/bin/bash

cp frontend mask.app/Contents/MacOS/mask.app
codesign -s Developer mask.app/Contents/libs/* --options runtime --entitlements entitlements.plist --timestamp -f 
codesign -s Developer 'mask.app/Contents/MacOS/MagicMask' --options runtime --entitlements entitlements.plist --timestamp -f 
codesign -s Developer mask.app --deep --options runtime --entitlements entitlements.plist --timestamp -f
