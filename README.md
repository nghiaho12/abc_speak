An app I wrote to encourage my kid to learn the ABC alphabet. It employs Vosk to recognize speech. When the spoken letter is recognized the app will highlight the it. 

For best results pronounce each letter longer than you normally would. For example, say "Aaaaa".

![screenshot](screenshot.png)

# Build instructions
## Prerequisite
This repo uses git LFS for the assets. Install it before cloning.
```
sudo apt install git-lfs
```

Download the Vosk model.
```
./download_vosk_mode.sh
```

Install Docker if you want to build for Android or web.
```
sudo apt install docker-ce
```

## Linux
Download and install SDL3 (https://github.com/libsdl-org/SDL/).

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

# Contact
nghiaho12@yahoo.com
