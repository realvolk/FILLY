use crate::render::Rect;

pub enum Constraint {
    Fixed(u16),
    Percent(u16),
    Flex(u16),
    Min(u16),
    Max(u16),
    Auto,
}

pub enum Direction {
    Vertical,
    Horizontal,
}

pub fn linear(area: Rect, direction: Direction, constraints: &[Constraint]) -> Vec<Rect> {
    let mut rects = Vec::with_capacity(constraints.len());
    let total_fixed: u16 = constraints.iter().filter_map(|c| {
        if let Constraint::Fixed(v) = c { Some(*v) } else { None }
    }).sum();
    let available = match direction {
        Direction::Vertical => area.height.saturating_sub(total_fixed),
        Direction::Horizontal => area.width.saturating_sub(total_fixed),
    };
    let total_flex: u16 = constraints.iter().filter_map(|c| {
        if let Constraint::Flex(w) = c { Some(*w) } else { None }
    }).sum();
    let flex_unit = if total_flex > 0 { available / total_flex } else { 0 };
    let mut offset = match direction {
        Direction::Vertical => area.y,
        Direction::Horizontal => area.x,
    };
    for c in constraints {
        let size = match c {
            Constraint::Fixed(v) => *v,
            Constraint::Flex(w) => w * flex_unit,
            Constraint::Percent(p) => {
                let base = match direction {
                    Direction::Vertical => area.height,
                    Direction::Horizontal => area.width,
                };
                (*p as u32 * base as u32 / 100) as u16
            }
            _ => available / constraints.len() as u16,
        };
        let child_rect = match direction {
            Direction::Vertical => Rect::new(area.x, offset, area.width, size),
            Direction::Horizontal => Rect::new(offset, area.y, size, area.height),
        };
        rects.push(child_rect);
        offset += size;
    }
    rects
}

pub fn grid(area: Rect, rows: usize, cols: usize, row_constraints: &[Constraint], col_constraints: &[Constraint]) -> Vec<Vec<Rect>> {
    let row_rects = linear(Rect::new(area.x, area.y, area.width, area.height), Direction::Vertical, row_constraints);
    let col_rects = linear(Rect::new(area.x, area.y, area.width, area.height), Direction::Horizontal, col_constraints);
    let mut grid = Vec::with_capacity(rows);
    for r in 0..rows {
        let mut row = Vec::with_capacity(cols);
        for c in 0..cols {
            let x = col_rects[c].x;
            let y = row_rects[r].y;
            let w = col_rects[c].width;
            let h = row_rects[r].height;
            row.push(Rect::new(x, y, w, h));
        }
        grid.push(row);
    }
    grid
}