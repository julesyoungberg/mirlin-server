use nannou::prelude::*;

mod audio;

fn main() {
    nannou::app(model).update(update).simple_window(view).run();
}

struct Model {
    audio_features: audio::AudioFeatures,
}

fn model(_app: &App) -> Model {
    Model {
        audio_features: audio::AudioFeatures::new(),
    }
}

fn update(_app: &App, model: &mut Model, _update: Update) {
    model.audio_features.update();
}

fn view(app: &App, model: &Model, frame: Frame) {
    // Prepare to draw.
    let draw = app.draw();

    // Clear the background to purple.
    draw.background().color(PLUM);

    println!("current features: {:?}", model.audio_features.current);

    if let Some(payload) = model.audio_features.current.get("payload") {
        if let Some(features) = payload.get("features") {
            let tristimulus = features
                .get("tristimulus.mean")
                .unwrap()
                .as_array()
                .unwrap();
            // tristimulus maps sound to color
            draw.background().rgb(
                tristimulus[0].as_f64().unwrap() as f32,
                tristimulus[1].as_f64().unwrap() as f32,
                tristimulus[2].as_f64().unwrap() as f32,
            );

            let loudness = features.get("loudness.mean").unwrap().as_array().unwrap()[0]
                .as_f64()
                .unwrap();
            println!("loudness: {:?}", loudness);
            let size = 2000.0 * loudness as f32 + 1.0;
            // Draw a blue ellipse with default size and position.
            let ellipse = draw.ellipse().w_h(size, size).color(STEELBLUE);
            // if it's onset change the color
            if let Some(onset_data) = features.get("onset").unwrap().as_array() {
                let onset = onset_data[0].as_f64().unwrap();
                if onset != 0.0 {
                    ellipse.color(YELLOW);
                }
            }
        }
    }

    // Write to the window frame.
    draw.to_frame(app, &frame).unwrap();
}
