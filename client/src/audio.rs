use cpal;
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use ringbuf::{Consumer, RingBuffer};
use serde_json;
use serde_json::{json, Value};
use std::string::ToString;
use std::thread;
use websocket::client::ClientBuilder;
use websocket::OwnedMessage;

const CONNECTION: &'static str = "ws://127.0.0.1:9002";

fn get_audio_device() -> cpal::Device {
    let host = cpal::default_host();
    host.default_input_device()
        .expect("no audio input device available")
}

fn get_mic_config(device: &cpal::Device) -> cpal::SupportedStreamConfig {
    // get supported config
    let mut supported_configs_range = device
        .supported_input_configs()
        .expect("error while querying audio configs");
    supported_configs_range
        .next()
        .expect("no supported audio config?!")
        .with_max_sample_rate()
}

pub struct AudioFeatures {
    pub consumer: Consumer<serde_json::Value>,
    pub current: serde_json::Value,
    pub recv_thread: std::thread::JoinHandle<()>,
    pub stream: cpal::Stream,
    pub onset: bool,
    pub loudness: f32,
    pub tristimulus: [f32; 3],
    pub smoothing: f32,
    pub mfcc: [f32; 13],
}

impl AudioFeatures {
    pub fn new() -> Self {
        println!("configuring audio input device");
        let audio_device = get_audio_device();
        let mic_config = get_mic_config(&audio_device);
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
                "features": [
                    "rms",
                    "energy",
                    "centroid",
                    "loudness",
                    "noisiness",
                    "pitch",
                    "mfcc",
                    "dissonance",
                    "key",
                    "tristimulus",
                    "spectral_contrast",
                    "spectral_complexity",
                    "onset",
                ],
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

        // create a ring buffer for server responses (features)
        let ring_buffer = RingBuffer::<serde_json::Value>::new(2);
        let (mut producer, consumer) = ring_buffer.split();
        producer.push(json!(null)).unwrap();
        // listen for messages from server, push to ring buffer
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

        Self {
            consumer,
            current: json!(null),
            recv_thread,
            stream,
            onset: false,
            loudness: 0.0,
            tristimulus: [0.0, 0.0, 0.0],
            smoothing: 0.6,
            mfcc: [
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            ],
        }
    }

    fn lerp(&self, prev: f32, next: f32) -> f32 {
        self.smoothing * prev + (1.0 - self.smoothing) * next
    }

    // update current value if new data is available
    pub fn update(&mut self) {
        self.current = match self.consumer.pop() {
            Some(v) => v,
            None => return,
        };

        let payload = match self.current.get("payload") {
            Some(p) => p,
            None => return,
        };

        let features = match payload.get("features") {
            Some(f) => f,
            None => return,
        };

        // println!("current features: {:?}", features);

        if let Some(onset) = features.get("onset").unwrap().as_array() {
            self.onset = onset[0].as_f64().unwrap() != 0.0;
        }

        let loudness = features.get("loudness.mean").unwrap().as_array().unwrap()[0]
            .as_f64()
            .unwrap();
        self.loudness = self.lerp(self.loudness, loudness as f32);

        let tristimulus = features
            .get("tristimulus.mean")
            .unwrap()
            .as_array()
            .unwrap();
        for i in 0..3 {
            self.tristimulus[i] =
                self.lerp(self.tristimulus[i], tristimulus[i].as_f64().unwrap() as f32);
        }

        let mfcc = features.get("mfcc.mean").unwrap().as_array().unwrap();
        for i in 0..13 {
            self.mfcc[i] = self.lerp(self.mfcc[i], mfcc[i].as_f64().unwrap() as f32);
        }
    }
}
