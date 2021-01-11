use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use cpal::Data;
use serde_json;
use serde_json::{json, Value};
use std::panic;
use std::string::ToString;
use std::thread;
use std::time;
use websocket::client::ClientBuilder;
use websocket::OwnedMessage;

const CONNECTION: &'static str = "ws://127.0.0.1:9002";

fn run() {
    println!("Connecting to {}", CONNECTION);

    let client = ClientBuilder::new(CONNECTION)
        .unwrap()
        .add_protocol("rust-websocket")
        .connect_insecure()
        .unwrap();

    println!("Successfully connected");

    let (mut receiver, mut sender) = client.split().unwrap();

    // build subscription request
    let subscription_request = json!({
        "type": "subscription_request",
        "payload": {
            "features": ["onset", "pitch"],
        }
    });

    // request subscription
    let request_message = OwnedMessage::Text(subscription_request.to_string());
    match sender.send_message(&request_message) {
        Ok(()) => (),
        Err(e) => {
            println!("Error requesting subscription: {:?}", e);
            return;
        }
    }

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
    println!("confirmation: {:?}", confirmation);

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

    // create the stream
    let stream = device
        .build_input_stream(
            &supported_config.config(),
            move |data: &[f32], _: &cpal::InputCallbackInfo| {
                println!(
                    "received {} bytes of audio input, forwarding to mirlin server",
                    data.len()
                );
            },
            move |err| {
                println!("error: {:?}", err);
            },
        )
        .unwrap();

    stream.play().unwrap();
    std::thread::sleep(std::time::Duration::from_secs(3));
    drop(stream);

    // TODO begin sending audio data and receiving features
    // spin up 2 threads for this like here
    // https://github.com/websockets-rs/rust-websocket/blob/master/examples/client.rs
}

fn main() {
    loop {
        let _result = panic::catch_unwind(run);
        println!("Sleeping for 10 seconds...");
        thread::sleep(time::Duration::from_secs(10));
    }
}
