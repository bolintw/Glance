// Case B: a closed box for the board, taped to the back of the frame. No
// screws: the lid drops in under two fixed eaves on the FPC side and two
// spring arms on the antenna side catch it, and posts under the lid hold
// the board down. The arms are the outer wall cut free, so they bend
// sideways within each printed layer. The antenna stays inside: plastic
// doesn't block WiFi.
//
// Print the base floor down and the lid top down (part = "lid" lays it
// out that way), both without supports. Tape the floor, but not under the
// arms. To open: lift the lid at the notch between the arms; the hooks'
// sloped undersides push the arms aside.
//
//   openscad -o base.stl -D 'part="base"' case_back_mount.scad
//   openscad -o lid.stl -D 'part="lid"' case_back_mount.scad

include <glance_v31.scad>

gap = 0.3;        // around the board
wall = 2.0;
floor_t = 1.2;
standoff = 2.5;   // floor to the board's bottom: clears the pins (1.9)
headroom = 5.55;  // board top to the lid: the tallest part (BAT, 5.05) + 0.5
lid_t = 1.6;
lid_gap = 0.2;    // around the lid

z_top = floor_t + standoff + pcb_t;
lid_z = z_top + headroom;           // the lid's underside
catch_z = lid_z + lid_t + 0.15;     // the hooks' and eaves' undersides

// The hooks reach over the lid's edge by `hook_p`; their undersides slope
// up at `hook_under` degrees so the lid can be pried out.
hook_p = 1.0;
hook_under = 30;
hook_tip = 0.55;
cap_z = catch_z + hook_p * tan(hook_under) + hook_tip + hook_p;  // the walls' top

cavity = [[-gap, -gap], [board[0] + gap, antenna_y + 0.77]];  // room for the antenna
cavity_r = board_r + gap;
y_top = cavity[1][1];
y_bot = cavity[0][1];

arm_t = 1.2;
arm_gap = wall - arm_t;   // between an arm and the cavity, so it's flush outside
arm_slot = 1.0;           // past its free end
// [fixed end, free end, hook from, hook to] along the antenna-side wall.
arms = [
    [25.0, 4.0, 4.0, 10.0],
    [33.5, 54.5, 48.5, 54.5],
];
eave_cover = 1.2;         // over the lid's edge
eaves = [[4, 14], [45.5, 54.5]];

// Under the board, clear of the pins: [x0, y0, x1, y1].
pads = [
    [9, 0.3, 13, 4], [48, 0.3, 52, 4],
    [3.5, 36.6, 9, 39.2], [51, 35, 55, 39.2],
    [0, 18, 4, 22], [54, 26, 57.7, 30], [20, 18, 24, 22],
];
// Past the board's top edge, so it can't slide toward the antenna.
stops = [[3.5, 8], [50, 54.5]];
// The lid's posts onto bare spots of the board: [x, y, diameter, height of
// what they land on]. The lid swings down about its FPC edge, so a post near
// that edge sweeps toward +Y on the way in: the one by the USB lands on the
// connector's shell rather than beside it.
posts = [[4.5, 38.7, 2.2, 0], [4.0, 7.3, 2.5, 3.26], [19, 4.5, 2.5, 0], [54.5, 3, 2.5, 0], [54.5, 37, 2.5, 0]];
notch = [27, 32];         // in the lid's edge between the arms, to pry it up

module cavity2d(grow = 0) {
    translate(cavity[0]) offset(r = cavity_r + grow) offset(delta = -cavity_r) square(cavity[1] - cavity[0]);
}

// A window through the wall, centered on (c, z), w wide and h tall.
module window_x(x_wall, c, z, w, h) {
    translate([x_wall - wall - 1, c - w / 2, z - h / 2]) cube([wall + 2, w, h]);
}

module base() {
    color("#6a8caf") difference() {
        union() {
            difference() {
                linear_extrude(cap_z) cavity2d(wall);
                translate([0, 0, floor_t]) linear_extrude(cap_z) cavity2d();
            }
            for (p = pads) translate([p[0], p[1], floor_t - 0.01]) cube([p[2] - p[0], p[3] - p[1], standoff + 0.01]);
            for (s = stops) translate([s[0], board[1] + gap, floor_t - 0.01]) cube([s[1] - s[0], 1.5, z_top - 0.1 - floor_t]);
        }
        window_x(-gap, usb_center_y, z_top + usb_center_z, usb_plug[0], usb_plug[1]);
        window_x(-gap, (bat_span[0] + bat_span[1]) / 2, z_top + 2.65, bat_span[1] - bat_span[0], 5.5);
        window_x(board[0] + gap + wall, (ext_span[0] + ext_span[1]) / 2, z_top + 2.65, ext_span[1] - ext_span[0], 5.5);
        translate([fpc_span[0], y_bot - wall - 1, z_top - 0.3]) cube([fpc_span[1] - fpc_span[0], wall + 2, 3.3]);
        // Free each arm from the wall and the floor.
        for (a = arms) {
            lo = min(a[0], a[1]);
            hi = max(a[0], a[1]);
            x0 = a[1] < a[0] ? lo - arm_slot : lo;
            x1 = a[1] < a[0] ? hi : hi + arm_slot;
            translate([x0, y_top - 0.01, -1]) cube([x1 - x0, wall + 1, cap_z + 2]);
            translate([a[0], y_top + arm_gap / 2, -1]) cylinder(d = arm_gap, h = cap_z + 2, $fn = 24);
        }
    }
    for (a = arms) arm(a);
    for (e = eaves) eave(e[0], e[1]);
}

module arm(a) {
    lo = min(a[0], a[1]);
    hi = max(a[0], a[1]);
    face = y_top + arm_gap;
    tip = y_top - hook_p;
    under = catch_z + hook_p * tan(hook_under);
    color("#e8913a") {
        translate([lo, face, 0]) cube([hi - lo, arm_t, cap_z]);
        along_x(a[2], a[3]) polygon([
            [face + 0.01, catch_z], [y_top, catch_z], [tip, under], [tip, under + hook_tip],
            [y_top, under + hook_tip + hook_p], [y_top, cap_z], [face + 0.01, cap_z],
        ]);
    }
}

// Flat for 0.5 mm off the wall, then 45 degrees up: the lid tilts in.
module eave(x0, x1) {
    reach = eave_cover + lid_gap;
    color("#4caf50") along_x(x0, x1) polygon([
        [y_bot - 0.01, catch_z], [y_bot + 0.5, catch_z], [y_bot + reach, catch_z + reach - 0.5],
        [y_bot + reach, cap_z], [y_bot - 0.01, cap_z],
    ]);
}

// In place: underside at lid_z, posts down to the board.
module lid() {
    color("#cfd8dc") {
        translate([0, 0, lid_z]) linear_extrude(lid_t) difference() {
            cavity2d(-lid_gap);
            translate([notch[0], y_top - lid_gap - 1.2]) square([notch[1] - notch[0], 2]);
        }
        for (p = posts) translate([p[0], p[1], z_top + p[3] + 0.05]) cylinder(d = p[2], h = lid_z - z_top - p[3] - 0.05 + 0.01, $fn = 32);
    }
}

part = "assembly";  // "base", "lid" (laid out for printing), "assembly", "exploded"
obj = "";           // the EasyEDA OBJ export, for the board in "assembly"
if (part == "base") base();
else if (part == "lid") translate([0, 0, lid_z + lid_t]) rotate([180, 0, 0]) lid();  // turned over, not mirrored
else {
    base();
    translate([0, 0, z_top]) board_model(obj);
    translate([0, 0, part == "exploded" ? 25 : 0]) lid();
}
