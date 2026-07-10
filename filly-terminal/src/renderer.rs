use filly_core::render::{
    Alignment, BorderStyle, Orientation, Rect, RenderNode, RenderTree, TextStyle, TreeNode,
};
use ratatui::{
    layout::Rect as RRect,
    style::{Color, Modifier, Style},
    widgets::{Block, Borders, Clear, List, ListItem, ListState, Paragraph, Wrap},
    Frame,
};

pub fn render_tree(f: &mut Frame, node: &RenderTree, area: RRect) {
    // Guard against rendering outside the terminal buffer
    if area.x >= f.area().width || area.y >= f.area().height {
        return;
    }
    let area = clamp_rect(area, f.area());
    if area.width == 0 || area.height == 0 {
        return;
    }

    match node {
        RenderNode::Container {
            rect,
            background,
            border,
            padding,
            children,
            ..
        } => {
            let abs_rect = translate_rect(area, *rect);
            if let Some(bg) = background {
                f.render_widget(Clear, abs_rect);
                f.render_widget(
                    Paragraph::new("")
                        .style(Style::default().bg(str_to_color(bg)))
                        .wrap(Wrap { trim: true }),
                    abs_rect,
                );
            }
            if !matches!(border, BorderStyle::None) {
                let borders = match border {
                    BorderStyle::Single | BorderStyle::Rounded => Borders::ALL,
                    BorderStyle::Double => Borders::ALL,
                    _ => Borders::ALL,
                };
                f.render_widget(
                    Block::default()
                        .borders(borders)
                        .border_style(Style::default().fg(Color::Gray)),
                    abs_rect,
                );
            }
            let inner = RRect::new(
                abs_rect.x + padding.left as u16,
                abs_rect.y + padding.top as u16,
                abs_rect
                    .width
                    .saturating_sub(padding.left as u16 + padding.right as u16),
                abs_rect
                    .height
                    .saturating_sub(padding.top as u16 + padding.bottom as u16),
            );
            let inner = clamp_rect(inner, abs_rect);
            for child in children {
                render_tree(f, child, inner);
            }
        }
        RenderNode::Text {
            rect,
            content,
            alignment,
            style,
            ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            let rstyle = to_style(style);
            let align = match alignment {
                Alignment::Left => ratatui::layout::Alignment::Left,
                Alignment::Center => ratatui::layout::Alignment::Center,
                Alignment::Right => ratatui::layout::Alignment::Right,
            };
            f.render_widget(
                Paragraph::new(content.as_str())
                    .style(rstyle)
                    .alignment(align)
                    .wrap(Wrap { trim: true }),
                abs_rect,
            );
        }
        RenderNode::List {
            rect,
            items,
            selected,
            highlight_style,
            ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            let ritems: Vec<ListItem> =
                items.iter().map(|i| ListItem::new(i.label.clone())).collect();
            let mut state = ListState::default().with_selected(*selected);
            let list = List::new(ritems)
                .highlight_style(to_style(highlight_style))
                .highlight_symbol("> ");
            f.render_stateful_widget(list, abs_rect, &mut state);
        }
        RenderNode::Input {
            rect,
            text,
            cursor,
            placeholder,
            masked,
            ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            let display = if text.is_empty() {
                placeholder.clone()
            } else {
                if *masked {
                    "*".repeat(text.len())
                } else {
                    text.clone()
                }
            };
            let style = if text.is_empty() {
                Style::default().fg(Color::DarkGray)
            } else {
                Style::default().fg(Color::Green)
            };
            f.render_widget(
                Paragraph::new(format!("> {}", display))
                    .style(style)
                    .wrap(Wrap { trim: true }),
                abs_rect,
            );
            if !text.is_empty() && !*masked {
                let cx = (abs_rect.x + 2 + *cursor as u16)
                    .min(abs_rect.x + abs_rect.width.saturating_sub(1));
                f.set_cursor_position((cx, abs_rect.y));
            }
        }
        RenderNode::Checkbox {
            rect,
            label,
            checked,
            focused,
            ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            let mark = if *checked { "[x]" } else { "[ ]" };
            let mut style = Style::default();
            if *focused {
                style = Style::default().fg(Color::Green).bg(Color::DarkGray);
            }
            f.render_widget(
                Paragraph::new(format!("{} {}", mark, label))
                    .style(style)
                    .wrap(Wrap { trim: true }),
                abs_rect,
            );
        }
        RenderNode::Toggle {
            rect,
            label,
            value,
            focused,
            ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            let text = format!(
                "[ {} ] {}",
                if *value { "ON" } else { "OFF" },
                label
            );
            let style = if *focused {
                Style::default().fg(Color::Green)
            } else {
                Style::default()
            };
            f.render_widget(
                Paragraph::new(text).style(style).wrap(Wrap { trim: true }),
                abs_rect,
            );
        }
        RenderNode::Spinner {
            rect,
            message,
            frame,
            ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            let frames = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"];
            let spin = frames[*frame % frames.len()];
            f.render_widget(
                Paragraph::new(format!("{} {}", spin, message))
                    .wrap(Wrap { trim: true }),
                abs_rect,
            );
        }
        RenderNode::Separator {
            rect, orientation, ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            match orientation {
                Orientation::Horizontal => {
                    let line = "─".repeat(abs_rect.width as usize);
                    f.render_widget(
                        Paragraph::new(line)
                            .style(Style::default().fg(Color::DarkGray))
                            .wrap(Wrap { trim: true }),
                        abs_rect,
                    );
                }
                Orientation::Vertical => {
                    for y in abs_rect.y..abs_rect.y + abs_rect.height {
                        f.render_widget(
                            Paragraph::new("│")
                                .style(Style::default().fg(Color::DarkGray))
                                .wrap(Wrap { trim: true }),
                            RRect::new(abs_rect.x, y, 1, 1),
                        );
                    }
                }
            }
        }
        RenderNode::Badge {
            rect, text, style, ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            f.render_widget(
                Paragraph::new(text.as_str())
                    .style(to_style(style))
                    .wrap(Wrap { trim: true }),
                abs_rect,
            );
        }
        RenderNode::Cursor { rect: _, x, y, .. } => {
            let abs_x = (area.x + x).min(area.x + area.width.saturating_sub(1));
            let abs_y = (area.y + y).min(area.y + area.height.saturating_sub(1));
            f.set_cursor_position((abs_x, abs_y));
        }
        RenderNode::Table {
            rect,
            headers,
            rows,
            selected_row,
            selected_col,
            highlight_style,
            ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            let header_text = headers.join("  ");
            f.render_widget(
                Paragraph::new(header_text)
                    .style(Style::default().fg(Color::Yellow))
                    .wrap(Wrap { trim: true }),
                RRect::new(abs_rect.x, abs_rect.y, abs_rect.width, 1),
            );
            let row_items: Vec<ListItem> = rows
                .iter()
                .enumerate()
                .map(|(ri, row)| {
                    let is_sel = selected_row == &Some(ri);
                    let line = row
                        .iter()
                        .enumerate()
                        .map(|(ci, cell)| {
                            if selected_col == &Some(ci) && is_sel {
                                format!("[{}]", cell)
                            } else {
                                cell.clone()
                            }
                        })
                        .collect::<Vec<_>>()
                        .join("  ");
                    ListItem::new(line)
                })
                .collect();
            let mut state = ListState::default().with_selected(*selected_row);
            let list = List::new(row_items)
                .highlight_style(to_style(highlight_style))
                .highlight_symbol("> ");
            f.render_stateful_widget(
                list,
                RRect::new(
                    abs_rect.x,
                    abs_rect.y + 1,
                    abs_rect.width,
                    abs_rect.height - 1,
                ),
                &mut state,
            );
        }
        RenderNode::Tree {
            rect,
            nodes,
            selected,
            highlight_style,
            ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            let mut flat = Vec::new();
            let mut sel_idx = None;
            flatten_tree(nodes, &mut flat, 0, selected, &mut sel_idx, 0);
            let ritems: Vec<ListItem> = flat
                .iter()
                .map(|(indent, label, _)| {
                    ListItem::new(format!(
                        "{:indent$}{}",
                        "",
                        label,
                        indent = indent * 2
                    ))
                })
                .collect();
            let mut state = ListState::default().with_selected(sel_idx);
            let list = List::new(ritems)
                .highlight_style(to_style(highlight_style))
                .highlight_symbol("> ");
            f.render_stateful_widget(list, abs_rect, &mut state);
        }
        RenderNode::Gauge {
            rect,
            percent,
            label,
            style,
            ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            let rstyle = to_style(style);
            let filled =
                (abs_rect.width as f32 * *percent as f32 / 100.0) as usize;
            let empty = abs_rect.width as usize - filled;
            let bar = format!("{}{}", "█".repeat(filled), "░".repeat(empty));
            f.render_widget(
                Paragraph::new(format!("{} {}%", label, percent))
                    .style(rstyle)
                    .wrap(Wrap { trim: true }),
                RRect::new(abs_rect.x, abs_rect.y, abs_rect.width, 1),
            );
            f.render_widget(
                Paragraph::new(bar).style(rstyle).wrap(Wrap { trim: true }),
                RRect::new(abs_rect.x, abs_rect.y + 1, abs_rect.width, 1),
            );
        }
        RenderNode::Calendar {
            rect,
            year,
            month,
            selected_day,
            highlight_style,
            ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            let days = days_in_month(*year, *month);
            let first_wd = first_weekday_of_month(*year, *month);
            let mut grid = String::from("Su Mo Tu We Th Fr Sa\n");
            for _ in 0..first_wd {
                grid.push_str("   ");
            }
            for d in 1..=days {
                if selected_day == &Some(d) {
                    grid.push_str(&format!("[{}]", d));
                } else {
                    grid.push_str(&format!(" {} ", d));
                }
                if (first_wd + d) % 7 == 0 && d != days {
                    grid.push('\n');
                }
            }
            f.render_widget(
                Paragraph::new(grid)
                    .style(to_style(highlight_style))
                    .wrap(Wrap { trim: true }),
                abs_rect,
            );
        }
        RenderNode::Form {
            rect,
            fields,
            focused,
            submit_label,
            ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            let mut y = abs_rect.y;
            for (i, field) in fields.iter().enumerate() {
                let label = format!("{}: ", field.label);
                let label_width = label.len() as u16;
                f.render_widget(
                    Paragraph::new(label)
                        .style(Style::default())
                        .wrap(Wrap { trim: true }),
                    RRect::new(abs_rect.x, y, abs_rect.width, 1),
                );
                let value = if i == *focused {
                    format!("> {}", field.value)
                } else {
                    field.value.clone()
                };
                f.render_widget(
                    Paragraph::new(value)
                        .style(if i == *focused {
                            Style::default().fg(Color::Green)
                        } else {
                            Style::default()
                        })
                        .wrap(Wrap { trim: true }),
                    RRect::new(
                        abs_rect.x + label_width,
                        y,
                        abs_rect.width - label_width,
                        1,
                    ),
                );
                y += 1;
            }
            let btn = format!("[ {} ]", submit_label);
            f.render_widget(
                Paragraph::new(btn)
                    .style(Style::default().fg(Color::Green))
                    .wrap(Wrap { trim: true }),
                RRect::new(abs_rect.x, y, abs_rect.width, 1),
            );
        }
        RenderNode::Tabs {
            rect,
            tabs,
            active,
            child,
            ..
        } => {
            let abs_rect = translate_rect(area, *rect);
            let tab_line: String = tabs
                .iter()
                .enumerate()
                .map(|(i, t)| {
                    if i == *active {
                        format!("[ {} ]", t)
                    } else {
                        format!("  {}  ", t)
                    }
                })
                .collect::<Vec<_>>()
                .join(" ");
            f.render_widget(
                Paragraph::new(tab_line)
                    .style(Style::default())
                    .wrap(Wrap { trim: true }),
                RRect::new(abs_rect.x, abs_rect.y, abs_rect.width, 1),
            );
            let child_h = abs_rect.height.saturating_sub(1);
            if child_h > 0 {
                let child_area = RRect::new(
                    abs_rect.x,
                    abs_rect.y + 1,
                    abs_rect.width,
                    child_h,
                );
                render_tree(f, child, child_area);
            }
        }
        RenderNode::SplitPanes {
            rect,
            orientation,
            split_position,
            first,
            second,
            ..
        } => {
            let abs_rect = translate_rect(area, *rect);
            let (first_area, second_area) = match orientation {
                Orientation::Horizontal => {
                    let first_w = (*split_position)
                        .min(abs_rect.width.saturating_sub(1))
                        .max(1);
                    let second_w = abs_rect.width.saturating_sub(first_w).max(1);
                    (
                        RRect::new(abs_rect.x, abs_rect.y, first_w, abs_rect.height),
                        RRect::new(
                            abs_rect.x + first_w,
                            abs_rect.y,
                            second_w,
                            abs_rect.height,
                        ),
                    )
                }
                Orientation::Vertical => {
                    let first_h = (*split_position)
                        .min(abs_rect.height.saturating_sub(1))
                        .max(1);
                    let second_h =
                        abs_rect.height.saturating_sub(first_h).max(1);
                    (
                        RRect::new(abs_rect.x, abs_rect.y, abs_rect.width, first_h),
                        RRect::new(
                            abs_rect.x,
                            abs_rect.y + first_h,
                            abs_rect.width,
                            second_h,
                        ),
                    )
                }
            };
            let first_area = clamp_rect(first_area, abs_rect);
            let second_area = clamp_rect(second_area, abs_rect);
            if first_area.width > 0 && first_area.height > 0 {
                render_tree(f, first, first_area);
            }
            if second_area.width > 0 && second_area.height > 0 {
                render_tree(f, second, second_area);
            }
        }
        RenderNode::ContextMenu {
            rect,
            items,
            selected,
            highlight_style,
            ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            let ritems: Vec<ListItem> =
                items.iter().map(|i| ListItem::new(i.label.clone())).collect();
            let mut state = ListState::default().with_selected(*selected);
            let list = List::new(ritems)
                .highlight_style(to_style(highlight_style))
                .highlight_symbol("> ");
            f.render_stateful_widget(list, abs_rect, &mut state);
        }
        RenderNode::Toast {
            rect,
            message,
            style,
            ..
        } => {
            let abs_rect = clamp_rect(translate_rect(area, *rect), area);
            if abs_rect.width == 0 || abs_rect.height == 0 {
                return;
            }
            f.render_widget(
                Paragraph::new(message.as_str())
                    .style(to_style(style))
                    .wrap(Wrap { trim: true }),
                abs_rect,
            );
        }
    }
}

fn clamp_rect(r: RRect, parent: RRect) -> RRect {
    let x = r.x.max(parent.x);
    let y = r.y.max(parent.y);
    let right = (r.x + r.width).min(parent.x + parent.width);
    let bottom = (r.y + r.height).min(parent.y + parent.height);
    RRect::new(
        x.min(parent.x + parent.width.saturating_sub(1)),
        y.min(parent.y + parent.height.saturating_sub(1)),
        right
            .saturating_sub(x)
            .min(parent.width.saturating_sub(x.saturating_sub(parent.x))),
        bottom
            .saturating_sub(y)
            .min(parent.height.saturating_sub(y.saturating_sub(parent.y))),
    )
}

fn translate_rect(parent: RRect, child: Rect) -> RRect {
    RRect::new(
        parent.x + child.x,
        parent.y + child.y,
        child.width,
        child.height,
    )
}

fn to_style(ts: &TextStyle) -> Style {
    let mut style = Style::default();
    if let Some(fg) = &ts.fg {
        style = style.fg(str_to_color(fg));
    }
    if let Some(bg) = &ts.bg {
        style = style.bg(str_to_color(bg));
    }
    if ts.bold {
        style = style.add_modifier(Modifier::BOLD);
    }
    if ts.italic {
        style = style.add_modifier(Modifier::ITALIC);
    }
    if ts.underline {
        style = style.add_modifier(Modifier::UNDERLINED);
    }
    style
}

fn str_to_color(s: &str) -> Color {
    match s {
        "white" => Color::White,
        "black" => Color::Black,
        "red" => Color::Red,
        "green" => Color::Green,
        "yellow" => Color::Yellow,
        "blue" => Color::Blue,
        "magenta" => Color::Magenta,
        "cyan" => Color::Cyan,
        "gray" => Color::Gray,
        "darkgray" => Color::DarkGray,
        "lightred" => Color::LightRed,
        "lightgreen" => Color::LightGreen,
        "lightyellow" => Color::LightYellow,
        "lightblue" => Color::LightBlue,
        "lightmagenta" => Color::LightMagenta,
        "lightcyan" => Color::LightCyan,
        _ => Color::White,
    }
}

fn flatten_tree(
    nodes: &[TreeNode],
    flat: &mut Vec<(usize, String, Vec<usize>)>,
    depth: usize,
    selected_path: &[usize],
    selected_idx: &mut Option<usize>,
    mut current_idx: usize,
) -> usize {
    for (i, node) in nodes.iter().enumerate() {
        let marker = if node.expanded { "▼" } else { "▶" };
        let label = format!("{} {}", marker, node.label);
        let mut path = selected_path.to_vec();
        path.push(i);
        if path == selected_path {
            *selected_idx = Some(current_idx);
        }
        flat.push((depth, label, path.clone()));
        current_idx += 1;
        if node.expanded {
            current_idx = flatten_tree(
                &node.children,
                flat,
                depth + 1,
                &path,
                selected_idx,
                current_idx,
            );
        }
    }
    current_idx
}

fn days_in_month(year: i32, month: u32) -> u32 {
    match month {
        1 | 3 | 5 | 7 | 8 | 10 | 12 => 31,
        4 | 6 | 9 | 11 => 30,
        2 => {
            if year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) {
                29
            } else {
                28
            }
        }
        _ => 30,
    }
}

fn first_weekday_of_month(year: i32, month: u32) -> u32 {
    let m = if month < 3 {
        month as i32 + 12
    } else {
        month as i32
    };
    let y = if month < 3 { year - 1 } else { year };
    let q: i32 = 1;
    let h = (q + (13 * (m + 1)) / 5 + y + y / 4 - y / 100 + y / 400) % 7;
    ((h + 6) % 7) as u32
}