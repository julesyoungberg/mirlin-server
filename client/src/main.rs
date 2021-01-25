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

    // tristimulus maps sound to color
    draw.background().rgb(
        model.audio_features.tristimulus[0],
        model.audio_features.tristimulus[1],
        model.audio_features.tristimulus[2],
    );

    let size = 5000.0 * model.audio_features.loudness + 5.0;
    // Draw a blue ellipse with default size and position.
    let ellipse = draw.ellipse().w_h(size, size).color(STEELBLUE);
    // if it's onset change the color
    if model.audio_features.onset {
        ellipse.color(YELLOW);
    }

    // Write to the window frame.
    draw.to_frame(app, &frame).unwrap();
}
