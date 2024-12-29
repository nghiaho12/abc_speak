FROM debian:12.8

RUN apt update && \
    apt install -y wget g++ git cmake libx11-dev libxext-dev libgl1-mesa-dev pkg-config libasound2-dev libpulse-dev \
    binutils coreutils desktop-file-utils fakeroot fuse patchelf python3-pip python3-setuptools squashfs-tools strace util-linux zsync

RUN pip3 install --break-system-packages appimage-builder==1.1.0

RUN git clone -b preview-3.1.6 --depth 1 https://github.com/libsdl-org/SDL.git
RUN cd SDL && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --config Release --parallel && \
    cmake --install . --config Release

RUN git clone -b 1.0.1 --depth 1 https://github.com/g-truc/glm.git
RUN cd glm && \
    cmake -DGLM_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF -B build . && \
    cmake --build build -- all && \
    cmake --build build -- install

WORKDIR /number_sequence_game
COPY src/ /number_sequence_game/src
COPY CMakeLists.txt /number_sequence_game

RUN mkdir build && \
    cd build && \
    cmake .. && \
    make && \
    make install

CMD ["/number_sequence_game/build/number_sequence_game"]
