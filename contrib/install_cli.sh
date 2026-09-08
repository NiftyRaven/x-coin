 #!/usr/bin/env bash

 # Execute this file to install the raven cli tools into your path on OS X

 CURRENT_LOC="$( cd "$(dirname "$0")" ; pwd -P )"
 LOCATION=${CURRENT_LOC%XCoin-Qt.app*}

 # Ensure that the directory to symlink to exists
 sudo mkdir -p /usr/local/bin

 # Create symlinks to the cli tools
 sudo ln -s ${LOCATION}/XCoin-Qt.app/Contents/MacOS/xcoind /usr/local/bin/xcoind
 sudo ln -s ${LOCATION}/XCoin-Qt.app/Contents/MacOS/xcoin-cli /usr/local/bin/xcoin-cli
