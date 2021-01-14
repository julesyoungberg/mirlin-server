# mirlin server

A real time music information retrieval server powered by essentia.

Analysis can be initiated by another application, and then the server will begin to accept frames of audio data. The server will continuously return extracted features.

Frame-by-frame analysis is chosen to avoid cross platform problems with the server listening to the microphone. Instead, the client must supply their own audio. This project is initially planned as a component of [rusty vision](https://github.com/julesyoungberg/rusty-vision).

## setup

Requires Docker, but comes with most dependencies as include files. To start:

```
make up
```

To run the example rust client:

```
cd client
cargo run --release
```
