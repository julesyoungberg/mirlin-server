use serde_json;
use serde_json::{json, Value};
use std::panic;
use std::string::ToString;
use std::thread;
use std::time;
use websocket::client::ClientBuilder;
use websocket::{Message, OwnedMessage};

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

    let subscription_request = json!({
        "type": "subscription_request",
        "payload": {
            "features": ["onset", "pitch"],
        }
    });

    let request_message = OwnedMessage::Text(subscription_request.to_string());

    match sender.send_message(&request_message) {
        Ok(()) => (),
        Err(e) => {
            println!("Error requesting subscription: {:?}", e);
            return;
        }
    }

    let confirmation_msg = match receiver.recv_message() {
        Ok(msg) => msg,
        Err(e) => {
            println!("Error confirming subscription: {:?}", e);
            return;
        }
    };

    let confirmation: Value = match confirmation_msg {
        OwnedMessage::Text(json_string) => serde_json::from_str(&json_string).unwrap(),
        _ => {
            println!("Invalid confirmation message");
            return;
        }
    };

    println!("confirmation: {:?}", confirmation);

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
