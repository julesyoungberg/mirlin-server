# mirlin server

A real time music information retrieval server powered by essentia.

Analysis can be initiated by another application, and then the server will begin to accept frames of audio data. The server will continuously return extracted features.

Frame-by-frame analysis is chosen to avoid problems with the server listening to the microphone from a docker container. Instead, the client must supply their own audio. 

This project is initially planned as a component of [rusty vision](https://github.com/julesyoungberg/rusty-vision).

## setup

Requires docker. To start:

```
make up
```

To run the example rust client:

```
cd client
cargo run --release
```

For Windows, you need to install ASIO as described here: https://crates.io/crates/cpal

For development, installing Essentia locally is recommended. 

## note

You must grant microphone access to the terminal you run the clients from, otherwise the input buffer will be only 0s.
