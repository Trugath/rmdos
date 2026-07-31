"""
Unit tests for starfield demo algorithms (CGA mode 4 grid, Bresenham, border circle).

Mirrors the logic in firmware/src/dos/starfield.c so we can test without the emulator.
"""

from __future__ import annotations

import unittest

# Constants from starfield.c
CGA_OFFSET_MAX = 0x4000
CGA_ROW_BYTES = 80
CGA_BANK_SIZE = 0x2000
CENTER_X = 158
CENTER_Y = 100
CENTER_COL = 39
SCREEN_W = 320
SCREEN_H = 200


def offset_to_x(off: int) -> int:
    if off < CGA_BANK_SIZE:
        bank = off // CGA_ROW_BYTES
        col = off - bank * CGA_ROW_BYTES
    else:
        off = off - CGA_BANK_SIZE
        bank = off // CGA_ROW_BYTES
        col = off - bank * CGA_ROW_BYTES
    return col * 4


def offset_to_y(off: int) -> int:
    if off < CGA_BANK_SIZE:
        return 2 * (off // CGA_ROW_BYTES)
    return 2 * ((off - CGA_BANK_SIZE) // CGA_ROW_BYTES) + 1


def xy_to_offset(x: int, y: int) -> int:
    col = x // 4
    if col >= CGA_ROW_BYTES:
        col = CGA_ROW_BYTES - 1
    if col < 0:
        col = 0
    if (y & 1) == 0:
        row_half = y // 2
        return row_half * CGA_ROW_BYTES + col
    row_half = y // 2
    return CGA_BANK_SIZE + row_half * CGA_ROW_BYTES + col


def col_y_to_offset(col: int, y: int) -> int:
    if col < 0:
        col = 0
    if col >= CGA_ROW_BYTES:
        col = CGA_ROW_BYTES - 1
    if y < 0:
        y = 0
    if y >= SCREEN_H:
        y = SCREEN_H - 1
    if (y & 1) == 0:
        row_half = y // 2
        return row_half * CGA_ROW_BYTES + col
    row_half = y // 2
    return CGA_BANK_SIZE + row_half * CGA_ROW_BYTES + col


def bresenham_next_offset(col: int, y: int, cc: int, cy: int) -> int:
    """One Bresenham step toward (cc, cy). Symmetric error-based algorithm."""
    if col == cc and y == cy:
        return 0
    dx = abs(cc - col)
    sx = 1 if col < cc else -1
    dy = -abs(cy - y)
    sy = 1 if y < cy else -1
    err = dx + dy
    e2 = 2 * err
    next_col = col + (sx if e2 >= dy else 0)
    next_y = y + (sy if e2 <= dx else 0)
    next_col = max(0, min(CGA_ROW_BYTES - 1, next_col))
    next_y = max(0, min(SCREEN_H - 1, next_y))
    if next_col == cc and next_y == cy:
        return 0
    return col_y_to_offset(next_col, next_y)


def steps_to_center(off: int) -> int:
    col = offset_to_x(off) // 4
    y = offset_to_y(off)
    adc = abs(col - CENTER_COL)
    ady = abs(y - CENTER_Y)
    return max(adc, ady)


def border_circle_point(i: int, n: int = 64, scale: int = 128, cos64: int = 127, sin64: int = 12) -> tuple[int, int]:
    """Ray from center at angle 2*pi*i/n; return (x,y) where it hits the rectangle."""
    import math
    cx, cy = CENTER_X, CENTER_Y
    angle = 2 * math.pi * i / n
    dx = scale * math.cos(angle)
    dy = scale * math.sin(angle)
    if abs(dx) < 1e-9 and abs(dy) < 1e-9:
        return (cx, cy)
    t_min = float("inf")
    x, y = cx, cy
    if dx > 0:
        t = (319 - cx) / dx
        if t > 0 and t < t_min:
            t_min = t
            x, y = 319, cy + dy * t
    elif dx < 0:
        t = (0 - cx) / dx
        if t > 0 and t < t_min:
            t_min = t
            x, y = 0, cy + dy * t
    if dy > 0:
        t = (199 - cy) / dy
        if t > 0 and t < t_min:
            t_min = t
            x, y = cx + dx * t, 199
    elif dy < 0:
        t = (0 - cy) / dy
        if t > 0 and t < t_min:
            t_min = t
            x, y = cx + dx * t, 0
    x = max(0, min(SCREEN_W - 1, round(x)))
    y = max(0, min(SCREEN_H - 1, round(y)))
    return (x, y)


class TestStarfieldOffsetRoundTrip(unittest.TestCase):
    """offset_to_x/y and xy_to_offset / col_y_to_offset round-trips."""

    def test_offset_zero_is_top_left(self) -> None:
        self.assertEqual(offset_to_x(0), 0)
        self.assertEqual(offset_to_y(0), 0)

    def test_offset_to_xy_and_back(self) -> None:
        for off in (0, 1, 79, 80, 0x2000, 0x2000 + 79, 4000, 0x2000 + 4000):
            with self.subTest(off=off):
                x = offset_to_x(off)
                y = offset_to_y(off)
                self.assertIn(x, range(0, SCREEN_W, 4), f"off={off} -> x={x}")
                self.assertIn(y, range(SCREEN_H))
                back = xy_to_offset(x, y)
                self.assertEqual(back, off, f"off={off} -> ({x},{y}) -> {back}")

    def test_col_y_to_offset_round_trip(self) -> None:
        for col in (0, 39, 79):
            for y in (0, 100, 199):
                off = col_y_to_offset(col, y)
                self.assertEqual(offset_to_x(off) // 4, col, f"col={col} y={y}")
                self.assertEqual(offset_to_y(off), y)


class TestStarfieldBresenham(unittest.TestCase):
    """Bresenham one-step toward center."""

    def test_step_toward_center_reduces_distance(self) -> None:
        for col, y in ((0, 100), (79, 100), (39, 0), (39, 199)):
            off = col_y_to_offset(col, y)
            next_off = bresenham_next_offset(col, y, CENTER_COL, CENTER_Y)
            if next_off == 0:
                self.assertEqual((col, y), (CENTER_COL, CENTER_Y), "only at center")
                continue
            d_before = steps_to_center(off)
            d_after = steps_to_center(next_off)
            self.assertLessEqual(d_after, d_before + 1)
            self.assertGreaterEqual(d_before - d_after, 0, "distance should not increase")

    def test_at_center_returns_zero(self) -> None:
        off_center = col_y_to_offset(CENTER_COL, CENTER_Y)
        next_off = bresenham_next_offset(CENTER_COL, CENTER_Y, CENTER_COL, CENTER_Y)
        self.assertEqual(next_off, 0)

    def test_step_from_left_edge_moves_right(self) -> None:
        col, y = 0, 100
        next_off = bresenham_next_offset(col, y, CENTER_COL, CENTER_Y)
        self.assertNotEqual(next_off, 0)
        next_col = offset_to_x(next_off) // 4
        self.assertEqual(next_col, 1)
        self.assertEqual(offset_to_y(next_off), 100)


class TestStarfieldStepsToCenter(unittest.TestCase):
    """steps_to_center (grid distance to center)."""

    def test_center_is_zero(self) -> None:
        off = col_y_to_offset(CENTER_COL, CENTER_Y)
        self.assertEqual(steps_to_center(off), 0)

    def test_left_edge_distance(self) -> None:
        off = col_y_to_offset(0, CENTER_Y)
        self.assertEqual(steps_to_center(off), CENTER_COL)

    def test_right_edge_distance(self) -> None:
        off = col_y_to_offset(79, CENTER_Y)
        self.assertEqual(steps_to_center(off), 79 - CENTER_COL)

    def test_top_edge_distance(self) -> None:
        off = col_y_to_offset(CENTER_COL, 0)
        self.assertEqual(steps_to_center(off), CENTER_Y)


class TestStarfieldBorderCircle(unittest.TestCase):
    """Border circle: ray from center hits rectangle boundary."""

    def test_border_points_on_rectangle(self) -> None:
        for i in range(64):
            x, y = border_circle_point(i)
            self.assertIn(x, range(SCREEN_W), f"angle {i}")
            self.assertIn(y, range(SCREEN_H), f"angle {i}")
            on_edge = x == 0 or x == SCREEN_W - 1 or y == 0 or y == SCREEN_H - 1
            self.assertTrue(on_edge, f"angle {i} ({x},{y}) should be on border")

    def test_border_angle_zero_is_right(self) -> None:
        x, y = border_circle_point(0)
        self.assertEqual(x, SCREEN_W - 1)
        self.assertEqual(y, CENTER_Y)

    def test_border_angle_16_is_bottom(self) -> None:
        x, y = border_circle_point(16)
        self.assertEqual(y, SCREEN_H - 1)

    def test_border_angle_32_is_left(self) -> None:
        x, y = border_circle_point(32)
        self.assertEqual(x, 0)


class TestStarfieldInitDistribution(unittest.TestCase):
    """Stars are placed on random border rays (not LCG x,y modulo lattices)."""

    @staticmethod
    def rnd_next(s: int) -> int:
        return (s * 25173 + 13849) & 0x7FFF

    def _init_stars(self, n: int = 120) -> list[tuple[int, int]]:
        """Mirror firmware ray-depth placement with a simple Euclidean walk."""
        s = 42
        out: list[tuple[int, int]] = []
        for i in range(n):
            s = self.rnd_next(s)
            bi = s % 128
            bx, by = border_circle_point(bi, n=128, scale=128, cos64=128, sin64=6)
            dist = max(abs(CENTER_X - bx), abs(CENTER_Y - by))
            max_in = max(3, dist - 10)
            s = self.rnd_next(s)
            steps = 3 + (s % max_in)
            # Approximate inward position along the ray.
            if dist <= 0:
                x, y = bx, by
            else:
                t = min(steps, dist - 1) / float(dist)
                x = int(round(bx + (CENTER_X - bx) * t))
                y = int(round(by + (CENTER_Y - by) * t))
            out.append((x, y))
            s = self.rnd_next(s)
        return out

    def test_not_all_x_multiples_of_four(self) -> None:
        xs = [x for x, _ in self._init_stars()]
        non_aligned = [x for x in xs if x % 4 != 0]
        self.assertGreater(len(non_aligned), len(xs) // 4, "stars should use full pixel X, not col*4")

    def test_covers_multiple_quadrants(self) -> None:
        pts = self._init_stars()
        q = [0, 0, 0, 0]
        for x, y in pts:
            qi = (0 if x < CENTER_X else 1) + (0 if y < CENTER_Y else 2)
            q[qi] += 1
        self.assertTrue(all(c > 5 for c in q), f"expected spread across quadrants, got {q}")

    def test_not_on_outer_frame(self) -> None:
        for x, y in self._init_stars():
            self.assertFalse(x <= 0 or x >= SCREEN_W - 1 or y <= 0 or y >= SCREEN_H - 1)


if __name__ == "__main__":
    unittest.main()
