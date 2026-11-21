// Case H: case A's open tray for the board, with a bay for the battery
// beside it, for inside the photo frame. Nothing is glued to the board or
// the pouch, so both come out again; only the floor is taped down. The LED
// button isn't in it: its plug goes into EXT, which the tray leaves open,
// and its wire runs out of the frame.
//
//   y
//   ^  .----------------------------------------.
//   |  |  slack |  battery (2000: 70 x 50)       |
//   |  |========|================================|  <- strap
//   |  |  lead  |                                |
//   |  |========|================================|  <- strap
//   |  |  .---------------------------------------'  <- low rib, clear of
//   |  |  |     [antenna]           |                   the antenna
//   |  BAT      board              EXT
//   |  USB                          |
//   |  '__________[FPC]_____________'
//   +---------------------------------------------> x
//
// The board half is case A: the board tilts in under two eaves on the FPC
// side and snaps down past two spring arms on the antenna side.
//
// The battery bay is case F's, with the same inserts so both pouches end up
// with their tops level:
//   spacer_2000  a 1 mm plate under the 2000 mAh pouch, with a lip at its end
//   filler_1000  a block filling the rest of the bay round the 1000 mAh
//                pouch, but for a strip along the rib for the lead
// Two straps across the bay hold the pouch (and the insert) down. Each
// tilts in under an eave on the USB-end wall, and its far end snaps down
// past a spring arm cut from the far wall. To take one out, push its arm
// outward and lift the end.
//
// The battery's lead (from a corner, 7 cm) leaves the pouch at its USB-side
// end, coils in the slack pocket, crosses the rib's gap into the strip
// between the bay and the board, and leaves the tray there (no wall) to the
// BAT plug just below.
//
// The tallest parts are the board's BAT and EXT sockets (10.35 mm above the
// floor); the arms and eaves stay under them, so it's as thick as case A.
//
// Print the base floor down, the inserts and the straps flat; no supports.
// Every arm stands on the bed by its foot alone: a brim ear at its free end
// keeps it from lifting. Tape the floor, but not under the arms.
//
//   openscad -o base.stl        -D 'part="base"'        case_in_frame_battery.scad
//   openscad -o strap.stl       -D 'part="strap"'       case_in_frame_battery.scad   (print two)
//   openscad -o spacer_2000.stl -D 'part="spacer_2000"' case_in_frame_battery.scad
//   openscad -o filler_1000.stl -D 'part="filler_1000"' case_in_frame_battery.scad

include <glance_v31.scad>

// --- The board's tray: case A ---

gap = 0.3;        // around the board
wall = 1.6;
floor_t = 1.2;
standoff = 2.5;   // floor to the board's bottom: clears the pins (1.9)

z_bot = floor_t + standoff;
z_top = z_bot + pcb_t;
wall_h = z_top - 0.1;     // below the board's top, so plugs and the antenna pass over

catch_z = z_top + 0.15;   // underside of the hooks and eaves
hook_p = 1.0;             // how far a hook reaches in
hook_tip = 0.55;          // the hook's upright face
cap_z = catch_z + hook_tip + hook_p;

arm_t = 1.4;      // A's 1.2 lifted at the free end on the bed (case B, F)
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

y_top = board[1] + gap;   // the board's cavity, antenna-side edge
y_bot = -gap;             // and FPC-side edge
tray_out = gap + wall;

// --- The battery's bay: case F's ---

bay_wall = 2.0;
rib = [antenna_y + 1.0, antenna_y + 2.2];  // y from, to
rib_h = 4.0;
rib_x0 = 6.0;                              // the lead crosses before it
bay = [[-gap, rib[1]], [82.0, rib[1] + 50 + 0.6]];
bay_r = 1.0;                               // inside corners: tight, so the pouch reaches them
bay_out = [bay[0] - [bay_wall, bay_wall], bay[1] + [bay_wall, bay_wall]];
low_x1 = 46.0;     // the bay's board-side wall is only the low rib up to here (antenna, lead)

// Pouches: [x0, y0, z0] in place, and size. Both against the far end and
// the rib, lead corner at the USB end by the rib.
p2000 = [[bay[1][0] - 0.3 - 70, rib[1] + 0.3, floor_t + 1.0], [70, 50, 5]];
p1000 = [[bay[1][0] - 0.3 - 52, rib[1] + 0.3, floor_t], [52, 34, 6]];
bat_top = floor_t + 6.0;                    // both pouches' tops
spacer_lip = p2000[0][0] - 1.0;             // the lip's USB-side face
lead_gap = 6.0;                             // in the 2000's lip, along the rib
lead_strip = 12.0;                          // the 1000's filler: left free along the rib
pocket_extra = 1.0;                         // the 1000's pocket: 0.3 was too tight (F)

// --- The straps ---

strap_w = 8.0;
strap_t = 1.2;
strap_z = bat_top + 0.2;                    // underside: just clear of the pouch
strap_end = 0.2;                            // short of each end wall
straps = [55, 77];                          // centres along y: both over either pouch
strap_rib = [1.6, 1.4];                     // a stiffening rib on top: width, height

bay_h = strap_z + strap_t;                  // the bay's walls: up to the straps' top
s_catch = bay_h + 0.15;
s_cap = s_catch + hook_tip + hook_p;

// Arms in the far wall: [fixed end, free end, hook from, hook to] along y.
s_arms = [[70, 51.5, 51.5, 58.5], [95, 73.5, 73.5, 80.5]];
// Eaves on the USB-end wall, over each strap's end.
s_eaves = [for (c = straps) [c - strap_w / 2 + 0.5, c + strap_w / 2 - 0.5]];

// --- Helpers ---

module rr2d(a, b, r) { translate(a) offset(r = r) offset(delta = -r) square(b - a); }
module bay2d(grow = 0) { rr2d(bay[0] - [grow, grow], bay[1] + [grow, grow], max(bay_r + grow, 0.01)); }

// A 2D profile (x -> X, y -> Z) swept along Y from y0 to y1.
module along_y(y0, y1) {
    translate([0, y1, 0]) rotate([90, 0, 0]) linear_extrude(y1 - y0) children();
}

module outline2d() {
    offset(r = tray_out) board2d();
    rr2d(bay_out[0], bay_out[1], board_r + tray_out);
    // The strip between the tray and the bay.
    translate([-tray_out, board[1] - 2]) square([board[0] + 2 * tray_out, bay_out[0][1] - board[1] + 3]);
}

// --- Parts ---

module base() {
    color("#6a8caf") difference() {
        union() {
            linear_extrude(floor_t) outline2d();
            // The tray's walls.
            difference() {
                linear_extrude(wall_h) offset(r = tray_out) board2d();
                translate([0, 0, floor_t]) linear_extrude(wall_h) offset(r = gap) board2d();
            }
            for (p = pads) translate([p[0], p[1], floor_t - 0.01]) cube([p[2] - p[0], p[3] - p[1], standoff + 0.01]);
            // The bay's walls, but the board side only from low_x1 on.
            difference() {
                linear_extrude(bay_h) rr2d(bay_out[0], bay_out[1], board_r + tray_out);
                translate([0, 0, floor_t]) linear_extrude(bay_h) bay2d();
                translate([bay_out[0][0] - 1, bay_out[0][1] - 1, floor_t]) cube([low_x1 - bay_out[0][0] + 1, bay_wall + 1.01, bay_h]);
            }
            translate([rib_x0, rib[0], floor_t - 0.01]) cube([low_x1 - rib_x0 + 0.01, rib[1] - rib[0], rib_h + 0.01]);
        }
        // USB: room for the cable's overmold.
        translate([-tray_out - 1, usb_center_y - usb_plug[0] / 2, z_top + usb_center_z - usb_plug[1] / 2])
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
        for (a = s_arms) {
            lo = min(a[0], a[1]);
            hi = max(a[0], a[1]);
            y0 = a[1] < a[0] ? lo - arm_slot : lo;
            y1 = a[1] < a[0] ? hi : hi + arm_slot;
            translate([bay[1][0] - arm_slot, y0, -1]) cube([arm_slot + bay_wall + 1, y1 - y0, 20]);
            translate([bay[1][0] - arm_slot / 2, a[0], -1]) cylinder(d = arm_slot, h = floor_t + 1.01, $fn = 24);
        }
    }
    for (a = arms) arm(a);
    for (e = eaves) eave(e[0], e[1]);
    for (a = s_arms) strap_arm(a);
    for (e = s_eaves) strap_eave(e[0], e[1]);
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

// The bay's arms: the far wall cut free, bending in X within each layer.
module strap_arm(a) {
    lo = min(a[0], a[1]);
    hi = max(a[0], a[1]);
    xi = bay[1][0];
    color("#e8913a") {
        translate([xi, lo, 0]) cube([arm_t, hi - lo, bay_h]);
        along_y(a[2], a[3]) polygon([
            [xi + arm_t, bay_h - 0.2], [xi, bay_h - 0.2], [xi, s_catch],
            [xi - hook_p, s_catch], [xi - hook_p, s_catch + hook_tip],
            [xi, s_cap], [xi + arm_t, s_cap],
        ]);
    }
}

module strap_eave(y0, y1) {
    xi = bay[0][0];
    reach = eave_cover + strap_end;
    color("#4caf50") along_y(y0, y1) polygon([
        [xi - bay_wall, bay_h - 0.2], [xi, bay_h - 0.2], [xi, s_catch],
        [xi + 0.5, s_catch], [xi + reach, s_catch + reach - 0.5],
        [xi + reach, s_cap], [xi - bay_wall, s_cap],
    ]);
}

// In place, centred on y = c.
module strap(c) {
    x0 = bay[0][0] + strap_end;
    x1 = bay[1][0] - strap_end;
    color("#8d6e63") translate([0, c, strap_z]) {
        translate([x0, -strap_w / 2, 0]) cube([x1 - x0, strap_w, strap_t]);
        // Clear of the eave and the hook, and of the eave as the strap tilts in.
        translate([x0 + 5, -strap_rib[0] / 2, strap_t - 0.01]) cube([x1 - x0 - 10, strap_rib[0], strap_rib[1] + 0.01]);
    }
}

// In place on the bay's floor.
module spacer_2000() {
    y0 = bay[0][1] + 0.2;
    y1 = bay[1][1] - 0.2;
    color("#bcaaa4") intersection() { translate([0, 0, floor_t - 1]) linear_extrude(10) bay2d(-0.2); translate([0, 0, floor_t]) {
        translate([bay[0][0] + 0.2, y0, 0]) cube([bay[1][0] - bay[0][0] - 0.4, y1 - y0, 1.0]);
        translate([spacer_lip - 1.2, y0 + lead_gap, 0]) cube([1.2, y1 - y0 - lead_gap, 4.0]);
        // A fillet behind the lip, on the slack side.
        translate([spacer_lip - 1.19, y0 + lead_gap, 0]) rotate([90, 0, 0]) mirror([0, 0, 1]) linear_extrude(y1 - y0 - lead_gap)
            polygon([[0, 0.99], [-2.0, 0.99], [0, 3.0]]);
    } }
}

// A solid block filling the bay round the 1000 mAh pouch, but for a strip
// along the rib that the lead coils in (and crosses to the rib's gap), and
// a hole to lift it out by.
module filler_1000() {
    y0 = bay[0][1] + 0.2;
    y1 = bay[1][1] - 0.2;
    yp = p1000[0][1] + p1000[1][1] + 0.3 + pocket_extra;   // past the pouch
    xp = p1000[0][0] - 0.3 - pocket_extra;                 // before it
    h = 5.0;
    color("#bcaaa4") intersection() {
        translate([0, 0, floor_t - 1]) linear_extrude(10) bay2d(-0.2);
        translate([0, 0, floor_t]) difference() {
            translate([bay[0][0] + 0.2, y0, 0]) cube([bay[1][0] - bay[0][0] - 0.4, y1 - y0, h]);
            translate([xp, y0 - 1, -1]) cube([bay[1][0], yp - y0 + 1, h + 2]);
            translate([bay[0][0] - 1, y0 - 1, -1]) cube([xp - bay[0][0] + 2, lead_strip + 1, h + 2]);
            translate([12, y1 - 9, -1]) cylinder(d = 10, h = h + 2, $fn = 48);
        }
    }
}

module pouch(which) {
    p = which == "2000" ? p2000 : p1000;
    color("#d4e157") translate(p[0]) rounded_box(p[1], 1.5);
}

size = bay_out[1] - [min(bay_out[0][0], -tray_out), -tray_out];
echo(str("case H: ", size[0], " x ", size[1], " x ", max(s_cap, cap_z), " mm (sockets 10.35); strap ",
         strap_z, "-", bay_h + strap_rib[1], " over the pouch's ", bat_top));

part = "assembly";  // "base", "strap", "spacer_2000", "filler_1000" (laid out for printing), "assembly", "exploded", "none"
battery = "2000";
obj = "";
if (part == "base") base();
else if (part == "strap") translate([0, 0, -strap_z]) strap(0);
else if (part == "spacer_2000") translate([0, 0, -floor_t]) spacer_2000();
else if (part == "filler_1000") translate([0, 0, -floor_t]) filler_1000();
else if (part == "assembly" || part == "exploded") {
    e = part == "exploded";
    base();
    translate([0, 0, z_top + (e ? 15 : 0)]) board_model(obj);
    translate([0, 0, e ? 8 : 0]) { if (battery == "2000") spacer_2000(); else filler_1000(); }
    translate([0, 0, e ? 14 : 0]) pouch(battery);
    for (c = straps) translate([0, 0, e ? 22 : 0]) strap(c);
}
