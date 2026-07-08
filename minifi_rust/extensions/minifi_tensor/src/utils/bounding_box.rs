use image::{Rgb, RgbImage};
use minifi_native::{MinifiError, PropertyConstraints, PropertySchema, PropertyType};
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct BoundingBox {
    pub(crate) class_id: usize,
    pub(crate) confidence: f32,
    pub(crate) x_min: f32,
    pub(crate) y_min: f32,
    pub(crate) x_max: f32,
    pub(crate) y_max: f32,
}

fn draw_thick_rect(
    img: &mut RgbImage,
    left: u32,
    top: u32,
    right: u32,
    bottom: u32,
    thickness: u32,
    color: Rgb<u8>,
) {
    let box_width = right.saturating_sub(left);
    let box_height = bottom.saturating_sub(top);

    for t in 0..thickness {
        if box_width > 2 * t && box_height > 2 * t {
            let rect = imageproc::rect::Rect::at((left + t) as i32, (top + t) as i32)
                .of_size(box_width - 2 * t, box_height - 2 * t);
            imageproc::drawing::draw_hollow_rect_mut(img, rect, color);
        }
    }
}

impl BoundingBox {
    pub fn class_id(&self) -> usize {
        self.class_id
    }
    pub fn confidence(&self) -> f32 {
        self.confidence
    }

    pub(crate) fn calculate_intersection_over_union(box1: &BoundingBox, box2: &BoundingBox) -> f32 {
        let x_left = box1.x_min.max(box2.x_min);
        let y_top = box1.y_min.max(box2.y_min);
        let x_right = box1.x_max.min(box2.x_max);
        let y_bottom = box1.y_max.min(box2.y_max);

        if x_right < x_left || y_bottom < y_top {
            return 0.0;
        }

        let intersection_area = (x_right - x_left) * (y_bottom - y_top);
        let box1_area = (box1.x_max - box1.x_min) * (box1.y_max - box1.y_min);
        let box2_area = (box2.x_max - box2.x_min) * (box2.y_max - box2.y_min);

        intersection_area / (box1_area + box2_area - intersection_area)
    }

    pub(crate) fn apply_non_maximum_suppression(
        mut boxes: Vec<BoundingBox>,
        iou_threshold: f32,
    ) -> Vec<BoundingBox> {
        boxes.sort_by(|a, b| {
            b.confidence()
                .partial_cmp(&a.confidence())
                .unwrap_or(std::cmp::Ordering::Equal)
        });

        let mut keep = Vec::new();
        let mut is_suppressed = vec![false; boxes.len()];

        for i in 0..boxes.len() {
            if is_suppressed[i] {
                continue;
            }

            keep.push(boxes[i].clone());

            for j in (i + 1)..boxes.len() {
                if is_suppressed[j] {
                    continue;
                }

                if boxes[i].class_id() == boxes[j].class_id() {
                    let iou = BoundingBox::calculate_intersection_over_union(&boxes[i], &boxes[j]);
                    if iou > iou_threshold {
                        is_suppressed[j] = true;
                    }
                }
            }
        }
        keep
    }

    pub(crate) fn draw_onto(&self, img: &mut RgbImage, line_thickness: u32, line_color: Rgb<u8>) {
        let width = img.width() as f32;
        let height = img.height() as f32;

        let left = (self.x_min * width).round() as u32;
        let top = (self.y_min * height).round() as u32;
        let right = (self.x_max * width).round() as u32;
        let bottom = (self.y_max * height).round() as u32;

        draw_thick_rect(img, left, top, right, bottom, line_thickness, line_color);
    }
}

pub(crate) struct BoundingBoxes {}

impl PropertySchema for BoundingBoxes {
    const CONSTRAINT: Option<PropertyConstraints> = None;
    const IS_REQUIRED: bool = false;
}

impl PropertyType for BoundingBoxes {
    type Output = Vec<BoundingBox>;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        serde_json::from_str::<Vec<BoundingBox>>(s).map_err(MinifiError::other)
    }
}

#[cfg(test)]
mod tests {
    use crate::utils::bounding_box::BoundingBox;

    #[test]
    fn test_iou_zero_when_disjoint() {
        let a = BoundingBox {
            class_id: 0,
            confidence: 1.0,
            x_min: 0.0,
            y_min: 0.0,
            x_max: 1.0,
            y_max: 1.0,
        };
        let b = BoundingBox {
            class_id: 0,
            confidence: 1.0,
            x_min: 2.0,
            y_min: 2.0,
            x_max: 3.0,
            y_max: 3.0,
        };
        assert_eq!(BoundingBox::calculate_intersection_over_union(&a, &b), 0.0);
    }

    #[test]
    fn test_nms_suppresses_overlapping_same_class() {
        let boxes = vec![
            BoundingBox {
                class_id: 1,
                confidence: 0.9,
                x_min: 0.0,
                y_min: 0.0,
                x_max: 1.0,
                y_max: 1.0,
            },
            BoundingBox {
                class_id: 1,
                confidence: 0.8,
                x_min: 0.1,
                y_min: 0.1,
                x_max: 1.0,
                y_max: 1.0,
            },
            BoundingBox {
                class_id: 2,
                confidence: 0.7,
                x_min: 0.1,
                y_min: 0.1,
                x_max: 1.0,
                y_max: 1.0,
            },
        ];
        let kept = BoundingBox::apply_non_maximum_suppression(boxes, 0.5);
        assert_eq!(kept.len(), 2); // same-class dupe suppressed, other-class kept
    }
}
