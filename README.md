Simple number sequence game for kids written using SDL3 and OpenGL ES.
It rus on Ubuntu 24.04, Android 14, Firefox 130.0 and Chromimum 131.0.

![screenshot](screenshot.png)

# Install
## Prerequisite
This repo uses git LFS for the assets. Install it before cloning.
```
sudo apt install git-lfs
```

Install Docker if you want to build for Android or web.
```
sudo apt install docker-ce
```

For Linux, install SDL 3 (https://github.com/libsdl-org/SDL/).

## Linux
```
cmake -B build
cmake --build build
./build/number_sequence_game
```

Hit ESC to quit.

## Android
```
docker build -f Dockerfile.android -t number_sequence_game_android .
docker run --rm --network=host number_sequence_game_android
```

Point your Android web browser to http://[IP of host]:8000. Download and install the APK.

## Web
```
docker build -f Dockerfile.wasm -t number_sequence_game_wasm .
docker run --rm --network=host number_sequence_game_wasm
```

Point your browser to http://localhost:8000.

# Credits
Sound assets 
- https://opengameart.org/content/win-sound-effect
- https://kenney.nl/assets/ui-audio
- https://soundimage.org/funny-2

Ogg Vorbis decoder
- https://github.com/nothings/stb

Signed Distance Field (SDF) font
- https://github.com/Chlumsky/msdfgen

# Contact
nghiaho12@yahoo.com
