use nannou::prelude::*;

mod audio;
mod util;

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

    let size = util::clamp(3000.0 * model.audio_features.loudness + 5.0, 1.0, 200.0);
    // Draw a blue ellipse with default size and position.
    let ellipse = draw.ellipse().w_h(size, size).color(BLACK);
    // if it's onset change the color
    if model.audio_features.onset {
        ellipse.color(WHITE);
    }

    let colors = [RED, ORANGE, YELLOW, LIME, CYAN, INDIGO, VIOLET];
    for i in 0..7 {
        let radius = i as f32 * 40.0 + 120.0;

        let points = (0..=360).map(|j| {
            let radian = deg_to_rad(j as f32);
            let x = radian.sin() * radius;
            let y = radian.cos() * radius;
            (pt2(x, y), colors[i])
        });

        let scale = 300.0 * (i + 1) as f32;
        let weight = util::clamp(model.audio_features.mfcc[i + 1].abs() * scale, 1.0, 35.0);

        draw.polyline().weight(weight).points_colored(points);
    }

    // Write to the window frame.
    draw.to_frame(app, &frame).unwrap();
}
