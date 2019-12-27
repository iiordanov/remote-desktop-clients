# IOS Remote Desktop Clients

## bVNC

VNC client for iOS.

### Building Libraries
First, build dependent libraries. Determine your Development team ID from https://developer.apple.com/account/#/membership/

Then, pass it to `build-libs.sh`, optionally providing the type of build as a second parameter:

    ./build-libs.sh DEV_TEAM_ID Debug

### Developing

Open `bVNC.xcodeproj` in Xcode.
