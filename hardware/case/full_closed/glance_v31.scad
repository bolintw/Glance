// The Glance V3.1 board and shared helpers for its cases.
//
// Board coordinates: origin at the board's bottom-left corner (the USB
// side), X along the 58 mm edge, Y toward the ESP32 antenna, Z = 0 at the
// board's top surface. Read from the EasyEDA PCB on 2026-11-05: EasyEDA
// absolute (mm) = board + (122.85, -89.276).

board = [58, 40];
board_r = 3;
pcb_t = 1.6;
pin_below = 1.9;    // through-hole pins and their solder under the board
antenna_y = 45.53;  // the module's antenna sticks out past the top edge

// Parts that matter to a case: [name, x0, y0, x1, y1, height above the board].
parts = [
    ["U1 ESP32",  24.63, 19.25, 44.03, 45.53, 3.12],
    ["USB",        0.06,  2.35,  8.09, 12.20, 3.26],
    ["BAT",       -0.02, 30.91,  7.93, 37.24, 5.05],
    ["EXT_BTN",   51.01, 14.43, 58.00, 24.68, 5.05],  // no 3D model in EasyEDA: assumed as tall as BAT
    ["FPC",       27.35,  0.06, 43.95,  6.32, 2.00],
    ["BTN",       44.78, 36.74, 50.38, 39.97, 1.95],
    ["RST",        9.99, 35.72, 15.59, 38.95, 1.95],
    ["BOOT",      45.07, 11.94, 48.30, 17.54, 1.95],
    ["L1",        35.50, 11.77, 40.11, 16.17, 2.61],
    ["L2",         5.10, 13.98, 10.31, 20.56, 2.02],
];

// Through-hole pins sticking out of the board's bottom: [x, y].
pins = [
    [6.31, 35.03], [6.31, 33.04],                              // BAT
    [32.88, 30.19],                                            // U1 hand-solder hole
    [2.79, 11.60], [6.99, 11.60], [2.79, 2.95], [6.99, 2.95],  // USB shell legs
    [51.78, 22.56], [51.78, 20.56], [51.78, 18.56], [51.78, 16.56],  // EXT_BTN
];

// Where the plugs go in, along the board edge they face.
usb_center_y = 7.28;
usb_center_z = 1.63;     // above the board's top
usb_plug = [12.4, 6.6];  // a USB-C cable's overmold, width x height
bat_span = [30.41, 37.74];
ext_span = [13.93, 25.18];
fpc_span = [26.85, 44.45];

module board2d() {
    offset(r = board_r) offset(delta = -board_r) square(board);
}

// A 2D profile (x -> Y, y -> Z) swept along X from x0 to x1.
module along_x(x0, x1) {
    translate([x0, 0, 0]) rotate([90, 0, 90]) linear_extrude(x1 - x0) children();
}

module rounded_box(size, r) {
    hull() for (x = [r, size[0] - r], y = [r, size[1] - r]) translate([x, y, 0]) cylinder(r = r, h = size[2], $fn = 24);
}

// The board for previews: the EasyEDA 3D export when `obj` names it,
// otherwise boxes for the parts above. EXT_BTN is a box either way.
module board_model(obj = "") {
    color("#2e7d32") translate([0, 0, -pcb_t]) linear_extrude(pcb_t) board2d();
    if (obj != "") {
        color("#3a8f3e") translate([-122.85, 89.276, 0]) import(obj);
    } else {
        for (p = parts) color(p[0] == "U1 ESP32" ? "#b0b0b0" : "#444") translate([p[1], p[2], 0]) cube([p[3] - p[1], p[4] - p[2], p[5]]);
    }
    color("#f5f5f5") translate([51.01, 14.43, 0]) cube([6.99, 10.25, 5.05]);
}
