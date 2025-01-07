An app I wrote to encourage my kid to learn the ABC alphabet. 
When the spoken letter is recognized (via microphone) it will get highlighted.
The app will also respond the NATO phonetic alphabet (https://en.wikipedia.org/wiki/NATO_phonetic_alphabet)

For best results pronounce each letter longer than you normally would. For example, say "Aaaaa".

![screenshot](screenshot.png)

# Build instructions
## Prerequisite
This repo uses git LFS for the assets. Install it before cloning.
```
sudo apt install git-lfs
```

Download the Vosk English model.
```
./download_vosk_model.sh
```

## Linux
- Install SDL3 (https://github.com/libsdl-org/SDL/)
- Install Vosk API (https://alphacephei.com/vosk/install)

```
cmake -B build
cmake --build build
./build/abc_speak
```

Hit ESC to quit.

## Android
```
docker build -f Dockerfile.android -t abc_speak_android .
docker run --rm --network=host abc_speak_android
```

Point your Android web browser to http://[IP of host]:8000. Download and install the APK.

The APK is targeted at Android 9 (API Level 28) and above.

## Web
```
docker build -f Dockerfile.wasm -t abc_speak_wasm .
docker run --rm --network=host abc_speak_wasm
```

Point your browser to http://localhost:8000.

Hit F to toggle fullscreen.

# Contact
nghiaho12@yahoo.com
