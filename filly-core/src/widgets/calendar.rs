use crate::event::{Event, KeyCode};
use crate::render::{
    Alignment, BorderStyle, EdgeInsets, Rect, RenderNode, RenderTree, TextStyle,
};
use crate::widget::{EventResult, Widget};
use filly_protocol::WidgetResponse;

pub struct CalendarWidget {
    pub title: String,
    pub year: i32,
    pub month: u32,
    pub selected_day: u32,
    dirty: bool,
}

impl CalendarWidget {
    pub fn new(title: String) -> Self {
        Self {
            title,
            year: 2025,
            month: 1,
            selected_day: 1,
            dirty: true,
        }
    }

    fn days_in_month(&self) -> u32 {
        match self.month {
            1 | 3 | 5 | 7 | 8 | 10 | 12 => 31,
            4 | 6 | 9 | 11 => 30,
            2 => {
                if self.year % 4 == 0 && (self.year % 100 != 0 || self.year % 400 == 0) {
                    29
                } else {
                    28
                }
            }
            _ => 30,
        }
    }
}

impl Widget for CalendarWidget {
    fn render(&self, area: Rect) -> RenderTree {
        let box_w = 40u16.min(area.width.saturating_sub(2));
        let box_h = 12u16.min(area.height.saturating_sub(2));
        let box_x = (area.width.saturating_sub(box_w)) / 2;
        let box_y = (area.height.saturating_sub(box_h)) / 2;

        let mut children = Vec::new();
        children.push(RenderNode::Text {
            rect: Rect::new(1, 0, box_w - 2, 1),
            content: format!("{} {}", self.title, month_name(self.month)),
            alignment: Alignment::Center,
            style: TextStyle {
                bold: true,
                fg: Some("green".into()),
                ..TextStyle::normal()
            },
            role: None,
            label: None,
            description: None,
        });
        children.push(RenderNode::Calendar {
            rect: Rect::new(1, 1, box_w - 2, box_h - 3),
            year: self.year,
            month: self.month,
            selected_day: Some(self.selected_day),
            highlight_style: TextStyle::selected(),
            role: None,
            label: None,
            description: None,
        });
        children.push(RenderNode::Text {
            rect: Rect::new(1, box_h - 2, box_w - 2, 1),
            content: "Left/Right:day  Up/Down:week  Enter:select  Esc:cancel".into(),
            alignment: Alignment::Center,
            style: TextStyle::muted(),
            role: None,
            label: None,
            description: None,
        });

        RenderTree::Container {
            rect: Rect::new(box_x, box_y, box_w, box_h),
            background: None,
            border: BorderStyle::Single,
            padding: EdgeInsets::zero(),
            children,
            role: None,
            label: None,
            description: None,
        }
    }

    fn handle_event(&mut self, event: &Event) -> EventResult {
        match event {
            Event::Key(code, _) => match code {
                KeyCode::Esc => EventResult::Response(WidgetResponse {
                    result: None,
                    cancelled: true,
                    error: None,
                }),
                KeyCode::Left => {
                    if self.selected_day > 1 {
                        self.selected_day -= 1;
                        self.dirty = true;
                    }
                    EventResult::Handled
                }
                KeyCode::Right => {
                    if self.selected_day < self.days_in_month() {
                        self.selected_day += 1;
                        self.dirty = true;
                    }
                    EventResult::Handled
                }
                KeyCode::Up => {
                    self.selected_day = self.selected_day.saturating_sub(7).max(1);
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Down => {
                    self.selected_day =
                        (self.selected_day + 7).min(self.days_in_month());
                    self.dirty = true;
                    EventResult::Handled
                }
                KeyCode::Enter => {
                    let date = format!(
                        "{}-{:02}-{:02}",
                        self.year, self.month, self.selected_day
                    );
                    EventResult::Response(WidgetResponse {
                        result: Some(serde_json::Value::String(date)),
                        cancelled: false,
                        error: None,
                    })
                }
                _ => EventResult::Unhandled,
            },
            _ => EventResult::Unhandled,
        }
    }

    fn is_dirty(&self) -> bool {
        self.dirty
    }
    fn clear_dirty(&mut self) {
        self.dirty = false;
    }
}

fn month_name(m: u32) -> &'static str {
    match m {
        1 => "Jan",
        2 => "Feb",
        3 => "Mar",
        4 => "Apr",
        5 => "May",
        6 => "Jun",
        7 => "Jul",
        8 => "Aug",
        9 => "Sep",
        10 => "Oct",
        11 => "Nov",
        12 => "Dec",
        _ => "?",
    }
}