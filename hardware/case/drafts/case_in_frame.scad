// Case A: a tray for the board inside the photo frame. The board's front
// stays open; it drops in under two fixed eaves on the FPC side and is held
// on the antenna side by two spring arms cut from the wall. The arms bend
// sideways, within each printed layer, so PLA takes it. Print it as it
// sits, floor down, no supports. Tape the floor, but not under the arms.
//
//   openscad -o case_in_frame.stl case_in_frame.scad

include <glance_v31.scad>

gap = 0.3;        // around the board
wall = 1.6;       // 4 perimeters of a 0.4 nozzle
floor_t = 1.2;
standoff = 2.5;   // floor to the board's bottom: clears the pins (1.9)

z_bot = floor_t + standoff;
z_top = z_bot + pcb_t;
wall_h = z_top - 0.1;     // below the board's top, so plugs and the antenna pass over

catch_z = z_top + 0.15;   // underside of the hooks and eaves
hook_p = 1.0;             // how far a hook reaches in
hook_tip = 0.55;          // the hook's upright face
cap_z = catch_z + hook_tip + hook_p;

arm_t = 1.2;
arm_slot = 1.0;
// [fixed end, free end, hook from, hook to] along the antenna-side wall.
arms = [
    [23.5, 3.5, 3.5, 9.0],
    [34.2, 54.7, 51.0, 54.7],   // runs under the antenna, the hook clear of BTN
];
eave_cover = 1.2;               // over the board's edge
eaves = [[4, 14], [44.5, 54.5]];

// Under the board, clear of the pins: [x0, y0, x1, y1].
pads = [
    [9, 0.3, 13, 4], [48, 0.3, 52, 4],
    [3.5, 36.6, 9, 39.2], [51, 35, 55, 39.2],
    [0, 18, 4, 22], [54, 26, 57.7, 30], [20, 18, 24, 22],
];

y_top = board[1] + gap;   // the cavity's antenna-side edge
y_bot = -gap;             // the cavity's FPC-side edge

module tray() {
    color("#6a8caf") difference() {
        union() {
            difference() {
                linear_extrude(wall_h) offset(r = gap + wall) board2d();
                translate([0, 0, floor_t]) linear_extrude(wall_h) offset(r = gap) board2d();
            }
            for (p = pads) translate([p[0], p[1], floor_t - 0.01]) cube([p[2] - p[0], p[3] - p[1], standoff + 0.01]);
        }
        // USB: room for the cable's overmold.
        translate([-gap - wall - 1, usb_center_y - usb_plug[0] / 2, z_top + usb_center_z - usb_plug[1] / 2])
            cube([wall + 2, usb_plug[0], 10]);
        // Free each arm from the wall and the floor.
        for (a = arms) {
            lo = min(a[0], a[1]);
            hi = max(a[0], a[1]);
            x0 = a[1] < a[0] ? lo - arm_slot : lo;
            x1 = a[1] < a[0] ? hi : hi + arm_slot;
            translate([x0, y_top - arm_slot, -1]) cube([x1 - x0, arm_slot + wall + 1, 20]);
            translate([a[0], y_top - arm_slot / 2, -1]) cylinder(d = arm_slot, h = floor_t + 1.01, $fn = 24);
        }
    }
    for (a = arms) arm(a);
    for (e = eaves) eave(e[0], e[1]);
}

module arm(a) {
    lo = min(a[0], a[1]);
    hi = max(a[0], a[1]);
    color("#e8913a") {
        translate([lo, y_top, 0]) cube([hi - lo, arm_t, wall_h]);
        along_x(a[2], a[3]) polygon([
            [y_top + arm_t, wall_h - 0.2], [y_top, wall_h - 0.2], [y_top, catch_z],
            [y_top - hook_p, catch_z], [y_top - hook_p, catch_z + hook_tip],
            [y_top, cap_z], [y_top + arm_t, cap_z],
        ]);
    }
}

// Flat for 0.5 mm off the wall, then 45 degrees up: the board tilts in.
module eave(x0, x1) {
    reach = eave_cover + gap;
    color("#4caf50") along_x(x0, x1) polygon([
        [y_bot - wall, wall_h - 0.2], [y_bot, wall_h - 0.2], [y_bot, catch_z],
        [y_bot + 0.5, catch_z], [y_bot + reach, catch_z + reach - 0.5],
        [y_bot + reach, cap_z], [y_bot - wall, cap_z],
    ]);
}

part = "case";   // "case", or "assembly" with the board in place
obj = "";        // the EasyEDA OBJ export, for "assembly"
tray();
if (part == "assembly") translate([0, 0, z_top]) board_model(obj);
