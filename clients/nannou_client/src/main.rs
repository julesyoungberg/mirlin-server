use cpal;
use cpal::traits::{DeviceTrait, StreamTrait};
use nannou::prelude::*;
use ringbuf::{Consumer, RingBuffer};
use serde_json;
use serde_json::{json, Value};
use std::string::ToString;
use std::thread;
use websocket::client::ClientBuilder;
use websocket::OwnedMessage;

mod util;

const CONNECTION: &'static str = "ws://127.0.0.1:9002";

fn main() {
    nannou::app(model).update(update).simple_window(view).run();
}

#[allow(dead_code)]
struct Model {
    consumer: Consumer<serde_json::Value>,
    current: serde_json::Value,
    recv_thread: std::thread::JoinHandle<()>,
    stream: cpal::Stream,
}

fn model(_app: &App) -> Model {
    println!("configuring audio input device");
    let audio_device = util::get_audio_device();
    let mic_config = util::get_mic_config(&audio_device);
    let cpal::SampleRate(sample_rate) = mic_config.sample_rate();
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
                        println!("Error sending frame");
                        panic!(e);
                    }
                }
            },
            move |err| {
                panic!(err);
            },
        )
        .unwrap();

    stream.play().unwrap();

    let ring_buffer = RingBuffer::<serde_json::Value>::new(2); // Add some latency
    let (mut producer, consumer) = ring_buffer.split();
    producer.push(json!(null)).unwrap();

    let recv_thread = thread::spawn(move || {
        for raw in receiver.incoming_messages() {
            let message = match raw {
                Ok(m) => m,
                Err(e) => {
                    println!("Error receiving message: {:?}", e);
                    return;
                }
            };

            let value: Value = match message {
                OwnedMessage::Text(json_string) => serde_json::from_str(&json_string).unwrap(),
                _ => {
                    println!("Received unexpected message: {:?}", message);
                    return;
                }
            };

            producer.push(value).ok();
        }
    });

    Model {
        consumer,
        current: json!(null),
        recv_thread,
        stream,
    }
}

fn update(_app: &App, model: &mut Model, _update: Update) {
    let value = model.consumer.pop();
    if !value.is_none() {
        model.current = value.unwrap();
    }
}

fn view(_app: &App, model: &Model, frame: Frame) {
    println!("current features: {:?}", model.current);
    frame.clear(PURPLE);
}
