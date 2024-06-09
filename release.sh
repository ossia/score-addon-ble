#!/bin/bash
rm -rf release
mkdir -p release

cp -rf BLE *.{hpp,cpp,txt,json} LICENSE release/

mv release score-addon-ble
7z a score-addon-ble.zip score-addon-ble
