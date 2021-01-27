pub fn clamp(v: f32, min: f32, max: f32) -> f32 {
    if v < min {
        return min;
    }

    if v > max {
        return max;
    }

    return v;
}
