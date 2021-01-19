use cpal;
use cpal::traits::{DeviceTrait, StreamTrait};
use nannou::prelude::*;
use serde_json;
use serde_json::{json, Value};
use std::process;
use std::string::ToString;
use std::sync::mpsc::channel;
use websocket::client::ClientBuilder;
use websocket::OwnedMessage;

mod util;

const CONNECTION: &'static str = "ws://127.0.0.1:9002";

fn main() {
    nannou::app(model).update(update).simple_window(view).run();
}

struct Model {
    receiver: websocket::receiver::Reader<std::net::TcpStream>,
    rx: std::sync::mpsc::Receiver<websocket::OwnedMessage>,
    stream: cpal::Stream,
    tx: std::sync::mpsc::Sender<websocket::OwnedMessage>,
}

fn model(_app: &App) -> Model {
    println!("configuring audio input device");
    let audio_device = util::get_audio_device();
    let mic_config = util::get_mic_config(&audio_device);
    let sample_rate = mic_config.sample_rate();
    println!("sample_rate: {:?}", sample_rate);

    println!("Connecting to {}", CONNECTION);

    let client = ClientBuilder::new(CONNECTION)
        .unwrap()
        .add_protocol("rust-websocket")
        .connect_insecure()
        .unwrap();

    let (mut receiver, mut sender) = client.split().unwrap();

    // build subscription request
    let session_request = json!({
        "type": "session_request",
        "payload": {
            "features": ["centroid", "loudness", "noisiness", "pitch"],
            "sample_rate": sample_rate,
            "hop_size": 512 as u32, // happens to be cpal's buffer size
            "memory": 4 as u32, // rember 4 frames including current
        }
    });

    println!("Requesting subscription {:?}", session_request);

    // request subscription
    let request_message = OwnedMessage::Text(session_request.to_string());
    match sender.send_message(&request_message) {
        Ok(()) => (),
        Err(e) => panic!("Error requesting subscription: {:?}", e),
    }

    // wait for a confirmation from the server
    println!("Confirming subscription");
    let confirmation_msg = match receiver.recv_message() {
        Ok(msg) => msg,
        Err(e) => panic!("Error confirming subscription: {:?}", e),
    };

    // the value must be serialized JSON, otherwise it is invalid
    let confirmation: Value = match confirmation_msg {
        OwnedMessage::Text(json_string) => serde_json::from_str(&json_string).unwrap(),
        _ => panic!("Invalid confirmation message"),
    };

    // successful confirmation
    println!("confirmation received: {:?}", confirmation);

    let (tx, rx) = channel();
    let tx_1 = tx.clone();
    let tx_2 = tx.clone();

    // create the stream
    let stream = audio_device
        .build_input_stream(
            &mic_config.config(),
            move |data: &[f32], _: &cpal::InputCallbackInfo| {
                let frame_message = json!({
                    "type": "audio_frame",
                    "payload": data,
                });

                // send frame to server
                let message = OwnedMessage::Text(frame_message.to_string());
                match sender.send_message(&message) {
                    Ok(()) => (),
                    Err(e) => {
                        println!("Error sending frame: {:?}", e);
                        let _ = tx_1.send(OwnedMessage::Close(None));
                    }
                }
            },
            move |err| {
                println!("error: {:?}", err);
                let _ = tx_2.send(OwnedMessage::Close(None));
            },
        )
        .unwrap();

    stream.play().unwrap();

    Model {
        receiver,
        rx,
        stream,
        tx,
    }
}

fn update(_app: &App, model: &mut Model, _update: Update) {
    let mut close = false;
    let message = match model.receiver.recv_message() {
        Ok(m) => m,
        Err(e) => {
            println!("Error receiving message");
            drop(model.stream);
            panic!(e);
        }
    };

    let value: Value = match message {
        OwnedMessage::Text(json_string) => serde_json::from_str(&json_string).unwrap(),
        _ => {
            println!("Received: {:?}", message);
            drop(model.stream);
            panic!("Receied unexpected message");
        }
    };

    println!("received: {:?}", value);

    // listen to channel for messages from threads
    let message = match model.rx.recv() {
        Ok(m) => m,
        Err(e) => {
            drop(model.stream);
            panic!(e);
        }
    };

    match message {
        OwnedMessage::Close(_) => {
            drop(model.stream);
            process::exit(1);
        }
        _ => (),
    }
}

fn view(_app: &App, _model: &Model, frame: Frame) {
    frame.clear(PURPLE);
}
