An app I wrote to help my kid to learn the ABC alphabet. 
When the spoken letter is recognized (via mic) it will get highlighted.
The app recognizes the NATO phonetic alphabet (https://en.wikipedia.org/wiki/NATO_phonetic_alphabet)

For best results pronounce each letter longer than you normally would. For example, say "Aaaaa".

Keyboard shorcuts
- ESC to quit (only applies to desktop app)
- F to toggle fullscreen (applies to dekstop and web app)

![screenshot](screenshot.png)

# Binary release
Go to for binary release packages.

# Building from source
## Prerequisite
This repo uses git LFS for the assets. Install it before cloning, e.g. ```sudo apt install git-lfs```.
You'll also need to have Docker installed, e.g. ```sudo apt install docker-ce```.

## Linux
The default Linux target is Debian 12.8. Edit Dockerfile.linux to match your distro if you run into problems with binary.

```
docker build -f Dockerfile.windows -t abc_speak:linux .
docker run --rm -it --network=host abc_speak:linux
```
Go to http://localhost:8000 to download the release package.

The tarball comes with SDL3 and libvosk shared library bundled. Run the binary by calling
```
LD_LIBRARY_PATH=. ./abc_speak
```
## Android
The APK is targeted at Android 9 (API Level 28) and above.

```
docker build -f Dockerfile.android -t abc_speak:android .
docker run --rm --network=host abc_speak:android
```

Point your Android web browser to http://localhost:8000 to download the APK.

## Web
```
docker build -f Dockerfile.wasm -t abc_speak:wasm .
docker run --rm --network=host abc_speak:wasm
```

Point your browser to http://localhost:8000 to download the release package.
If you have Python installed you can run the app by calling

```
python3 -m http.server
```

inside the extracted folder and point your browser to http://localhost:8000.

# Contact
nghiaho12@yahoo.com
