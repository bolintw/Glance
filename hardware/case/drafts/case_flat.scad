// Case F: one flat box for the board, the battery and the LED button,
// taped to the back of the frame. The board sits exactly as in case B (the
// FPC from the frame only reaches that high); the battery lies beside it,
// past the antenna, and the button stands in the corner past EXT.
//
//   y
//   ^  .----------------------------------------.
//   |  |  slack |  battery (2000: 70 x 50)       |
//   |  .--------+--------------------------------|  <- low rib, clear of
//   |  |   lead     [antenna]            |       |     the antenna
//   |  |                                 |       |
//   |  BAT      board                  EXT       |
//   |  USB                               | [btn] |
//   |  '__________[FPC]__________________|_______'
//   +-----------------------------------------------> x
//
// The battery's lead (from a corner, 7 cm) leaves the pouch at its USB-side
// end, coils in the slack pocket, crosses the rib's gap into the strip above
// the board, and leaves through a notch in the USB-end wall (open at the
// top: lay it in) to the BAT window just below. Only the plug and a short
// loop are outside.
//
// Batteries sit on the floor of their bay, held by an insert, so both tops
// end up level and the lid's posts just meet them; nothing is glued:
//   spacer_2000  a 1 mm plate under the 2000 mAh pouch, with a lip at its end
//   filler_1000  a solid block filling the rest of the bay round the 1000
//                mAh pouch, but for a strip along the rib for the lead
//
// The lid goes in like case B's: under eaves on the FPC side, then down past
// two spring arms on the battery side. The arms are the outer wall cut free,
// bending sideways within each printed layer. To open: lift the lid at the
// notch between the arms; the hooks' sloped undersides push the arms aside.
// Tape the floor, but not under the arms. Each arm stands on the bed by its
// foot alone: a brim ear at its free end keeps it from lifting.
//
// The button (Omten TSD1265): four corner posts under its black base,
// L-shaped corners round it, a frame under the lid on top of the base. Only
// the cap shows, through a round hole; a thin skin of the lid covers the
// white part's corners.
//
// Print the base floor down, the lid top down, the inserts flat; no
// supports anywhere.
//
// Since the first build (2026-11-15):
//   - The button was crowded: its solder joints and wires pushed it up off
//     its posts. The floor under it is now open in a cross between the
//     posts, so the joints sit lower and the wires leave flat on all sides.
//   - The lid sagged over the battery: its posts stopped 1.3 mm short of
//     the pouches. They now stop 0.3 mm above, and two more stand over the
//     2000's USB-side half.
//   - The lid was hard to lift at the notch alone: a finger hole by it,
//     over the filler's own hole (so with the 1000 it goes to the floor).
//
//   openscad -o base.stl        -D 'part="base"'        case_flat.scad
//   openscad -o lid.stl         -D 'part="lid"'         case_flat.scad
//   openscad -o spacer_2000.stl -D 'part="spacer_2000"' case_flat.scad
//   openscad -o filler_1000.stl -D 'part="filler_1000"' case_flat.scad

include <glance_v31.scad>

gap = 0.3;
wall = 2.0;
floor_t = 1.2;
standoff = 2.5;
headroom = 5.55;
lid_t = 1.6;
lid_gap = 0.2;

z_top = floor_t + standoff + pcb_t;  // board top, as case B
lid_z = z_top + headroom;
catch_z = lid_z + lid_t + 0.15;
hook_p = 1.0;
hook_under = 30;
hook_tip = 0.55;
cap_z = catch_z + hook_p * tan(hook_under) + hook_tip + hook_p;

// The battery's bay, past a low rib clear of the antenna.
rib = [antenna_y + 1.0, antenna_y + 2.2];  // y from, to
rib_h = 4.0;
rib_x0 = 6.0;                              // the lead crosses before it
bay_y = [rib[1], rib[1] + 50 + 0.6];

cav = [[-gap, -gap], [82.0, bay_y[1]]];
cav_r = 1.0;      // inside corners: tight, so the pouch reaches them
out_r = board_r + gap + wall;
y_bot = cav[0][1];
y_top = cav[1][1];

// Pouches: [x0, y0, z0] in place, and size. Both against the far end and
// the rib, lead corner at the USB end by the rib.
p2000 = [[cav[1][0] - 0.3 - 70, rib[1] + 0.3, floor_t + 1.0], [70, 50, 5]];
p1000 = [[cav[1][0] - 0.3 - 52, rib[1] + 0.3, floor_t], [52, 34, 6]];
bat_top = floor_t + 6.0;                   // both pouches' tops (7.0 left the posts 1.3 short)
spacer_lip = p2000[0][0] - 1.0;             // the lip's USB-side face
lead_gap = 6.0;                             // in the 2000's lip, along the rib
lead_strip = 12.0;                          // the 1000's filler: left free along the rib
pocket_extra = 1.0;                         // the 1000's pocket: 0.3 was too tight

// The lead's notch in the USB-end wall, above the BAT window.
lead_notch = [42.5, 45.5];

// Case B's board supports, the lid's posts, and stops at the board's top and
// right edges (the room goes on past both).
pads = [
    [9, 0.3, 13, 4], [48, 0.3, 52, 4],
    [3.5, 36.6, 9, 39.2], [51, 35, 55, 39.2],
    [0, 18, 4, 22], [54, 26, 57.7, 30], [20, 18, 24, 22],
];
posts = [[4.5, 38.7, 2.2, 0], [4.0, 7.3, 2.5, 3.26], [19, 4.5, 2.5, 0], [54.5, 3, 2.5, 0], [54.5, 37, 2.5, 0]];
stops_top = [[3.5, 8], [50, 54.5]];
stops_right = [[1, 9], [28, 38]];
// Posts onto the battery: over the 1000 mAh pouch, so over either.
bat_posts = [[40, 58], [72, 58], [40, 75], [72, 75], [20, 65], [20, 85]];  // the last two: the 2000 only (over the 1000's filler)

// Case B's arms: [fixed end, free end, hook from, hook to], along the
// battery-side wall; eaves along the FPC side.
arm_t = 1.4;      // B's 1.2 lifted at the free end on the bed: a little more foot
arm_gap = wall - arm_t;
arm_slot = 1.0;
arms = [[30, 6, 6, 12], [52, 76, 70, 76]];
eaves = [[4, 14], [46, 56], [66, 78]];
eave_cover = 1.2;
pry = [38, 44];
grip = [41, 90];  // a finger hole in the lid by the notch, and the filler's hole under it
grip_d = 10;

// The button, in the corner past EXT.
btn_c = [cav[1][0] - 8.5, cav[0][1] + 11.5];
btn_skin = 0.6;
btn_z = lid_z + lid_t - btn_skin - 0.05 - 6.5;
btn_proud = btn_z + 10.05 - lid_z - lid_t;
btn_fit = 0.15;
btn_hole = 9.6;   // the lid's pocket: room to tilt in past the white part
btn_round = 8.7;
btn_tab_slot = 2.6;
btn_well = [6.4, 15.0];  // the floor's cross under the button: between the posts, out past the base

module rr2d(a, b, r) { translate(a) offset(r = r) offset(delta = -r) square(b - a); }
module cav2d(grow = 0) { rr2d(cav[0] - [grow, grow], cav[1] + [grow, grow], max(cav_r + grow, 0.01)); }
module outer2d() { rr2d(cav[0] - [wall, wall], cav[1] + [wall, wall], out_r); }

module window_x(c, z, w, h) {
    translate([cav[0][0] - wall - 1, c - w / 2, z - h / 2]) cube([wall + 2, w, h]);
}

module base() {
    color("#6a8caf") difference() {
        union() {
            difference() {
                linear_extrude(cap_z) outer2d();
                translate([0, 0, floor_t]) linear_extrude(cap_z) cav2d();
            }
            for (p = pads) translate([p[0], p[1], floor_t - 0.01]) cube([p[2] - p[0], p[3] - p[1], standoff + 0.01]);
            for (s = stops_top) translate([s[0], board[1] + gap, floor_t - 0.01]) cube([s[1] - s[0], 1.5, z_top - 0.1 - floor_t]);
            for (s = stops_right) translate([board[0] + gap, s[0], floor_t - 0.01]) cube([1.5, s[1] - s[0], z_top - 0.1 - floor_t]);
            translate([rib_x0, rib[0], floor_t - 0.01]) cube([cav[1][0] - rib_x0 + 0.01, rib[1] - rib[0], rib_h + 0.01]);
            button_seat();
        }
        window_x(usb_center_y, z_top + usb_center_z, usb_plug[0], usb_plug[1]);
        window_x((bat_span[0] + bat_span[1]) / 2, z_top + 2.65, bat_span[1] - bat_span[0], 5.5);
        translate([cav[0][0] - wall - 1, lead_notch[0], floor_t + 0.5]) cube([wall + 2, lead_notch[1] - lead_notch[0], cap_z]);
        translate([fpc_span[0], y_bot - wall - 1, z_top - 0.3]) cube([fpc_span[1] - fpc_span[0], wall + 2, 3.3]);
        // Room under the button for its solder joints and wires.
        translate([btn_c[0], btn_c[1], -1]) linear_extrude(floor_t + 1.01) {
            square([btn_well[0], btn_well[1]], center = true);
            square([btn_well[1], btn_well[0]], center = true);
        }
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

module eave(x0, x1) {
    reach = eave_cover + lid_gap;
    color("#4caf50") along_x(x0, x1) polygon([
        [y_bot - 0.01, catch_z], [y_bot + 0.5, catch_z], [y_bot + reach, catch_z + reach - 0.5],
        [y_bot + reach, cap_z], [y_bot - 0.01, cap_z],
    ]);
}

module button_seat() {
    c = 2.6;
    s = 6 + btn_fit;
    translate([btn_c[0], btn_c[1], floor_t - 0.01]) for (mx = [0, 1], my = [0, 1]) mirror([mx, 0, 0]) mirror([0, my, 0]) {
        translate([6 - c, 6 - c, 0]) cube([c, c, btn_z - floor_t + 0.01]);
        translate([s, s - 2.2, 0]) cube([1.0, 3.2, btn_z - floor_t + 2.0]);
        translate([s - 2.2, s, 0]) cube([3.2, 1.0, btn_z - floor_t + 2.0]);
    }
}

module btn_cross(sq) {
    square(sq, center = true);
    square([btn_tab_slot, 15.6], center = true);
}

// In place: underside at lid_z; posts down to the board and the battery, a
// frame down onto the button's black base.
module lid() {
    color("#cfd8dc") {
        difference() {
            union() {
                translate([0, 0, lid_z]) linear_extrude(lid_t) difference() {
                    cav2d(-lid_gap);
                    translate([pry[0], y_top - lid_gap - 1.2]) square([pry[1] - pry[0], 2]);
                }
                translate([btn_c[0], btn_c[1], btn_z + 3.8 + 0.05]) linear_extrude(lid_z - btn_z - 3.8) square(11.6, center = true);
            }
            translate([btn_c[0], btn_c[1], btn_z]) linear_extrude(lid_z + lid_t - btn_skin - btn_z) btn_cross(btn_hole);
            translate([btn_c[0], btn_c[1], lid_z]) cylinder(d = btn_round, h = 5, $fn = 64);
            translate([grip[0], grip[1], lid_z - 1]) cylinder(d = grip_d, h = lid_t + 2, $fn = 48);
        }
        for (p = posts) translate([p[0], p[1], z_top + p[3] + 0.05]) cylinder(d = p[2], h = lid_z - z_top - p[3] - 0.05 + 0.01, $fn = 32);
        for (p = bat_posts) translate([p[0], p[1], bat_top + 0.3]) cylinder(d = 4, h = lid_z - bat_top - 0.3 + 0.01, $fn = 32);
    }
}

// In place on the bay's floor.
module spacer_2000() {
    y0 = bay_y[0] + 0.2;
    y1 = bay_y[1] - 0.2;
    color("#bcaaa4") intersection() { translate([0, 0, floor_t - 1]) linear_extrude(10) cav2d(-0.2); translate([0, 0, floor_t]) {
        translate([cav[0][0] + 0.2, y0, 0]) cube([cav[1][0] - cav[0][0] - 0.4, y1 - y0, 1.0]);
        translate([spacer_lip - 1.2, y0 + lead_gap, 0]) cube([1.2, y1 - y0 - lead_gap, 4.0]);
        // A fillet behind the lip, on the slack side.
        translate([spacer_lip - 1.19, y0 + lead_gap, 0]) rotate([90, 0, 0]) mirror([0, 0, 1]) linear_extrude(y1 - y0 - lead_gap)
            polygon([[0, 0.99], [-2.0, 0.99], [0, 3.0]]);
    } }
}

// A solid block filling the bay round the 1000 mAh pouch, but for a strip
// along the rib that the lead coils in (and crosses to the rib's gap), and
// a hole to lift it out by (under the lid's finger hole).
module filler_1000() {
    y0 = bay_y[0] + 0.2;
    y1 = bay_y[1] - 0.2;
    yp = p1000[0][1] + p1000[1][1] + 0.3 + pocket_extra;   // past the pouch
    xp = p1000[0][0] - 0.3 - pocket_extra;                 // before it
    h = 5.0;
    color("#bcaaa4") intersection() {
        translate([0, 0, floor_t - 1]) linear_extrude(10) cav2d(-0.2);
        translate([0, 0, floor_t]) difference() {
            translate([cav[0][0] + 0.2, y0, 0]) cube([cav[1][0] - cav[0][0] - 0.4, y1 - y0, h]);
            translate([xp, y0 - 1, -1]) cube([cav[1][0], yp - y0 + 1, h + 2]);
            translate([cav[0][0] - 1, y0 - 1, -1]) cube([xp - cav[0][0] + 2, lead_strip + 1, h + 2]);
            translate([grip[0], grip[1], -1]) cylinder(d = grip_d, h = h + 2, $fn = 48);
        }
    }
}

module pouch(which) {
    p = which == "2000" ? p2000 : p1000;
    color("#d4e157") translate(p[0]) rounded_box(p[1], 1.5);
}

module button() {
    translate([btn_c[0], btn_c[1], btn_z]) {
        color("#222") translate([-6, -6, 0]) cube([12, 12, 3.8]);
        color("#eee") {
            translate([-3.95, -3.95, 3.8]) cube([7.9, 7.9, 2.7]);
            translate([-0.9, -7.55, 3.8]) cube([1.8, 15.1, 1.2]);
        }
        color("#fffde7") translate([0, 0, 7.1]) cylinder(d1 = 8.3, d2 = 7, h = 2.95, $fn = 48);
    }
}

echo(str("case F: ", cav[1][0] - cav[0][0] + 2 * wall, " x ", cav[1][1] - cav[0][1] + 2 * wall, " x ", cap_z,
         " mm; button cap ", btn_proud, " mm out; under the button ", btn_z - floor_t, " mm"));

part = "assembly";  // "base", "lid", "spacer_2000", "filler_1000" (laid out for printing), "assembly", "exploded", "none"
battery = "2000";
obj = "";
if (part == "base") base();
else if (part == "lid") translate([0, 0, lid_z + lid_t]) rotate([180, 0, 0]) lid();
else if (part == "spacer_2000") translate([0, 0, -floor_t]) spacer_2000();
else if (part == "filler_1000") translate([0, 0, -floor_t]) filler_1000();
else if (part == "assembly" || part == "exploded") {
    e = part == "exploded";
    base();
    translate([0, 0, z_top]) board_model(obj);
    button();
    translate([0, 0, e ? 20 : 0]) lid();
    translate([0, 0, e ? 10 : 0]) { if (battery == "2000") spacer_2000(); else filler_1000(); pouch(battery); }
}
