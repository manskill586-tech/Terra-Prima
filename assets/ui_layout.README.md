# UI Layout Config

Edit `assets/ui_layout.json` to move/resize HUD blocks without touching code.

## Element block fields
- `position`: `[x, y]` in normalized viewport coordinates (0..1).
- `pivot`: origin point inside the element (`[0,0]` top-left, `[0.5,0.5]` center, `[1,1]` bottom-right).
- `size`: normalized size relative to viewport.
- `min_size`: minimum pixel size.
- `max_size`: maximum pixel size.

Final rect is adaptive to resolution and clamped by `min_size/max_size`.

## Text block
- `default_size`, `small_size`, `title_size`.
- `outline_size`, `font_color`, `outline_color`.

## Toolbar block
Controls revolver drum behavior:
- `front_scale`, `back_scale`, `selected_boost`
- `alpha_front`, `alpha_back`
- `rotation_duration`
- `radius_x_factor`, `radius_y_factor`, `center_y_factor`

If values are wrong or missing, scripts fall back to built-in defaults.
