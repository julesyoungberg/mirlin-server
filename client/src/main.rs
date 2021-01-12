use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use serde_json;
use serde_json::{json, Value};
use std::panic;
use std::string::ToString;
use std::sync::mpsc::channel;
use std::thread;
use std::time;
use websocket::client::ClientBuilder;
use websocket::OwnedMessage;

const CONNECTION: &'static str = "ws://127.0.0.1:9002";

fn run() {
    println!("configuring audio input device");

    // get audio device
    let host = cpal::default_host();
    let device = host
        .default_input_device()
        .expect("no audio input device available");

    // get supported config
    let mut supported_configs_range = device
        .supported_input_configs()
        .expect("error while querying audio configs");
    let supported_config = supported_configs_range
        .next()
        .expect("no supported audio config?!")
        .with_max_sample_rate();
    let cpal::SampleRate(sample_rate) = supported_config.sample_rate();
    println!("sample_rate: {:?}", sample_rate);

    println!("Connecting to {}", CONNECTION);

    let client = ClientBuilder::new(CONNECTION)
        .unwrap()
        .add_protocol("rust-websocket")
        .connect_insecure()
        .unwrap();

    let (mut receiver, mut sender) = client.split().unwrap();

    // build subscription request
    let subscription_request = json!({
        "type": "subscription_request",
        "payload": {
            "features": ["onset", "pitch"],
            "sample_rate": sample_rate,
        }
    });

    println!("Requesting subscription {:?}", subscription_request);

    // request subscription
    let request_message = OwnedMessage::Text(subscription_request.to_string());
    match sender.send_message(&request_message) {
        Ok(()) => (),
        Err(e) => {
            println!("Error requesting subscription: {:?}", e);
            return;
        }
    }

    println!("Confirming subscription");

    // wait for a confirmation from the server
    let confirmation_msg = match receiver.recv_message() {
        Ok(msg) => msg,
        Err(e) => {
            println!("Error confirming subscription: {:?}", e);
            return;
        }
    };

    // the value must be serialized JSON, otherwise it is invalid
    let confirmation: Value = match confirmation_msg {
        OwnedMessage::Text(json_string) => serde_json::from_str(&json_string).unwrap(),
        _ => {
            println!("Invalid confirmation message");
            return;
        }
    };

    // successful confirmation
    println!("confirmation received: {:?}", confirmation);

    let (tx, rx) = channel();
    let tx_1 = tx.clone();
    let tx_2 = tx.clone();

    // create the stream
    let stream = device
        .build_input_stream(
            &supported_config.config(),
            move |data: &[f32], _: &cpal::InputCallbackInfo| {
                println!(
                    "received {} bytes of audio input, forwarding to mirlin server",
                    data.len()
                );

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
                        let _ = tx.send(OwnedMessage::Close(None));
                        return;
                    }
                }
            },
            move |err| {
                println!("error: {:?}", err);
                let _ = tx_1.send(OwnedMessage::Close(None));
                return;
            },
        )
        .unwrap();

    stream.play().unwrap();
    std::thread::sleep(std::time::Duration::from_secs(3));

    // Receive loop
    let receive_loop = thread::spawn(move || {
        for message in receiver.incoming_messages() {
            let message = match message {
                Ok(m) => m,
                Err(e) => {
                    println!("Error receiving message: {:?}", e);
                    let _ = tx_2.send(OwnedMessage::Close(None));
                    return;
                }
            };

            let value: Value = match message {
                OwnedMessage::Text(json_string) => serde_json::from_str(&json_string).unwrap(),
                _ => {
                    println!("Received unexpected message: {:?}", message);
                    let _ = tx_2.send(OwnedMessage::Close(None));
                    return;
                }
            };

            println!("received: {:?}", value);
            // TODO: do something awesome with the data
        }
    });

    // listen to channel for messages from threads
    loop {
        let message = match rx.recv() {
            Ok(m) => m,
            Err(e) => {
                println!("Error reading from channel: {:?}", e);
                break;
            }
        };

        match message {
            OwnedMessage::Close(_) => break,
            _ => (),
        }
    }

    println!("closing stream");
    drop(stream);
    let _ = receive_loop.join();
}

fn main() {
    loop {
        let _result = panic::catch_unwind(run);
        println!("Sleeping for 10 seconds...");
        thread::sleep(time::Duration::from_secs(10));
    }
}
