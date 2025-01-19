#!/bin/bash

docker build -f Dockerfile.linux -t abc_speak:linux .
docker run --rm -it -v /tmp:/tmp --network=host abc_speak:linux /bin/bash -c 'cp /abc_speak/build/release/* /tmp'

docker build -f Dockerfile.windows -t abc_speak:windows .
docker run --rm -it -v /tmp:/tmp --network=host abc_speak:windows /bin/bash -c 'cp /abc_speak/build/release/* /tmp'

docker build -f Dockerfile.wasm -t abc_speak:wasm .
docker run --rm -it -v /tmp:/tmp --network=host abc_speak:wasm /bin/bash -c 'cp /abc_speak/build/release/* /tmp'

docker build -f Dockerfile.android -t abc_speak:android .
docker run --rm -it -v /tmp:/tmp --network=host abc_speak:android /bin/bash -c 'cp /SDL/build/org.libsdl.abc_speak/app/build/outputs/apk/debug/*.apk /tmp'

