// Case E: one box for the board, the battery and the LED button, taped to
// the back of the frame. Upstairs is case B's room for the board, grown to
// the right for the button; downstairs, under the same footprint, a bay the
// battery tray slides into from the USB side.
//
// Four parts, no glue, no bridges:
//   tub   the floor and three walls, open on top and at the USB end. Prints
//         floor down; nothing spans anything. Inside, a 45-degree ledge runs
//         round the top of the bay, and two small keys stand on it near the
//         far end.
//   deck  upstairs' floor, with the USB-end wall (USB and BAT windows), the
//         board's pads and stops and the button's seat. Prints flat. Slides
//         in from the USB end on the ledges; for the last few mm lift it over
//         the keys and set it down, so the keys sit in notches in its edges
//         and take the USB plug's push and pull. Once the lid is on, it
//         presses the board onto the deck and the deck can't lift off them.
//   lid   goes in like case B's: under eaves on the FPC side first, then
//         pressed down past three arms on the antenna side. The arms are only
//         in the upper wall, so none stands on the bed. Prints top down.
//   tray  the battery's, one for each battery; floor down.
//
// The battery's lead leaves the tray through a notch in its face (open at
// the top: lay the lead in, then slide the tray home) and goes up the
// outside into the BAT window. The tray clicks in on two side arms and
// pulls out by the lip at its foot; the lid needn't come off for that.
// Nothing is glued to the battery. The 1000 mAh tray holds the smaller pouch
// in a pocket toward the FPC side.
//
// The pouch sits under the antenna with the 2000 mAh battery. Board B ran
// for days with its pouch beside the antenna at -35..-40 dBm, about the same
// as without, so that's accepted (see the project notes).
//
// The button (Omten TSD1265) stands in the top-right corner, upstairs: four
// corner posts under its black base, L-shaped corners round it, and a frame
// under the lid on top of the base. Only the cap shows, through a round
// hole; a thin skin of the lid covers the white part's corners.
//
//   openscad -o tub.stl       -D 'part="tub"'                     case_one.scad
//   openscad -o deck.stl      -D 'part="deck"'                    case_one.scad
//   openscad -o lid.stl       -D 'part="lid"'                     case_one.scad
//   openscad -o tray_2000.stl -D 'part="tray"' -D 'battery="2000"' case_one.scad
//   openscad -o tray_1000.stl -D 'part="tray"' -D 'battery="1000"' case_one.scad

include <glance_v31.scad>

battery = "2000";  // which tray: "2000" (70 x 50 x 5) or "1000" (52 x 34 x 6)

gap = 0.3;
wall = 2.0;        // upstairs
bay_wall = 1.6;    // downstairs
floor_t = 1.2;
bay_h = 8.0;       // 7 mm of tray and battery, 1 mm over it
mid_t = 1.2;
standoff = 2.5;
lid_t = 1.6;
lid_gap = 0.2;

z_bay = floor_t;                    // the bay's floor
z_mid = floor_t + bay_h;            // the deck's underside
z_up = floor_t + bay_h + mid_t;     // upstairs floor
z_top = z_up + standoff + pcb_t;    // board top
lid_z = z_top + 5.55;               // lid underside: BAT (5.05) + 0.5
catch_z = lid_z + lid_t + 0.15;
hook_p = 0.8;
hook_under = 30;
hook_tip = 0.55;
cap_z = catch_z + hook_p * tan(hook_under) + hook_tip + hook_p;

// Upstairs room, and the footprint: that grown by the wall.
cav = [[-gap, -gap], [80.0, 49.7]];
cav_r = board_r + gap;
lo = cav[0] - [wall, wall];
hi = cav[1] + [wall, wall];
out_r = cav_r + wall;
y_bot = cav[0][1];
y_top = cav[1][1];

// The bay, open at x = lo[0].
bay = [[lo[0], lo[1] + bay_wall], [hi[0] - bay_wall, hi[1] - bay_wall]];

// The deck: its play in the tub, the ledge under it (from the bay's walls),
// the keys that hold it.
deck_c = 0.15;
ledge = 1.2;
ledge_x0 = bay[0][0] + 1.6 + 0.3;       // behind the tray's face
keys = [73, 76];                        // x, on both long walls
key_p = 0.6;                            // out of the wall

// Case B's board supports and the lid's posts, unchanged.
pads = [
    [9, 0.3, 13, 4], [48, 0.3, 52, 4],
    [3.5, 36.6, 9, 39.2], [51, 35, 55, 39.2],
    [0, 18, 4, 22], [54, 26, 57.7, 30], [20, 18, 24, 22],
];
posts = [[4.5, 38.7, 2.2, 0], [4.0, 7.3, 2.5, 3.26], [19, 4.5, 2.5, 0], [54.5, 3, 2.5, 0], [54.5, 37, 2.5, 0]];
// Stops at the board's top edge (as case B) and, now that the room goes on
// past it, at its right edge, clear of the EXT plug.
stops_top = [[3.5, 8], [50, 54.5]];
stops_right = [[1, 9], [28, 38]];

arms = [[8, 16], [36, 44], [62, 70]];   // along the antenna-side wall
arm_t = 1.2;
arm_slot = 0.8;
arm_z0 = z_up + 1.0;                    // where they're rooted
eaves = [[4, 14], [46, 56], [62, 74]];  // along the FPC-side wall
eave_cover = 1.2;
pry = [24, 29];                         // notch in the lid's edge

// The button.
btn_c = [cav[1][0] - 8.5, cav[1][1] - 8.5];
btn_skin = 0.6;                         // lid over the white part's corners
btn_z = lid_z + lid_t - btn_skin - 0.05 - 6.5;   // its body's bottom
btn_proud = btn_z + 10.05 - lid_z - lid_t;       // cap above the lid
btn_fit = 0.15;
btn_hole = 9.0;                         // round the white part, under the skin
btn_round = 8.7;                        // the cap's hole
btn_tab_slot = 2.6;                     // the LED tabs, along Y

// The tray.
tray_c = 0.1;
tray_h = bay_h - 0.3;
front = 10.0;                           // from the bay's mouth: arms, lead
arm_w = 0.9;                            // the tray's arms
arm_room = 0.8;
detent = [bay[0][0] + front - 1.0, 1.0, 0.5];  // x of the bump's middle, its length, height
lead_slot = [33, 37];                   // y, under the BAT window
bat = battery == "2000" ? [70, 50, 5] : [52, 34, 6];
tray_floor = 7.0 - bat[2];              // both batteries' tops end up level
bat_x0 = bay[0][0] + front + 1.3;       // after the stop rib

module rr2d(a, b, r) { translate(a) offset(r = r) offset(delta = -r) square(b - a); }
module outer2d() { rr2d(lo, hi, out_r); }
module cav2d(grow = 0) { rr2d(cav[0] - [grow, grow], cav[1] + [grow, grow], cav_r + grow); }

// Upstairs, open at the USB end: straight through, so the deck slides out.
module mouth2d() { translate([lo[0] - 1, y_bot]) square([cav[0][0] + cav_r - lo[0] + 1, y_top - y_bot]); }

// The deck's USB-end wall.
module end2d() {
    difference() {
        intersection() {
            outer2d();
            translate([lo[0] - 1, y_bot + deck_c]) square([cav[0][0] + cav_r - lo[0] + 1, y_top - y_bot - 2 * deck_c]);
        }
        cav2d();
    }
}

// A 45-degree ledge on a bay wall, along the wall from p0 to p1, standing
// out toward n.
module ledge_run(p0, p1, n) {
    len = norm(p1 - p0);
    d = (p1 - p0) / len;
    translate([p0[0], p0[1], z_mid]) multmatrix([[d[0], n[0], 0, 0], [d[1], n[1], 0, 0], [0, 0, 1, 0]])
        rotate([90, 0, 90]) linear_extrude(len) polygon([[-0.01, 0], [ledge, 0], [-0.01, -ledge - 0.01]]);
}

module tub() {
    notch_x = [detent[0] - detent[1] / 2 - 0.7, detent[0] + detent[1] / 2 + 0.7];
    color("#6a8caf") difference() {
        union() {
            difference() {
                linear_extrude(cap_z) outer2d();
                translate([0, 0, z_mid]) linear_extrude(cap_z) { cav2d(); mouth2d(); }
                translate([bay[0][0] - 1, bay[0][1], z_bay]) cube([bay[1][0] - bay[0][0] + 1, bay[1][1] - bay[0][1], bay_h + 0.01]);
            }
            // Ledges for the deck: both long walls (not over the tray's
            // notches) and the far end.
            for (r = [[ledge_x0, notch_x[0]], [notch_x[1], bay[1][0]]]) {
                ledge_run([r[0], bay[0][1]], [r[1], bay[0][1]], [0, 1]);
                ledge_run([r[1], bay[1][1]], [r[0], bay[1][1]], [0, -1]);
            }
            ledge_run([bay[1][0], bay[0][1]], [bay[1][0], bay[1][1]], [-1, 0]);
            // The keys.
            for (y = [y_bot - 0.01, y_top - key_p])
                translate([keys[0], y, z_mid - 0.01]) cube([keys[1] - keys[0], key_p + 0.01, mid_t + 0.01]);
        }
        // The notches the tray's arms click into.
        for (y = [bay[0][1] - 0.8, bay[1][1] - 0.01])
            translate([detent[0] - detent[1] / 2 - 0.2, y, z_bay - 0.01]) cube([detent[1] + 0.4, 0.81, tray_h]);
        // The FPC slot.
        translate([fpc_span[0], lo[1] - 1, z_top - 0.3]) cube([fpc_span[1] - fpc_span[0], wall + 2, 3.3]);
        // Free the arms: a slot each side, and thin them from outside.
        for (a = arms) {
            for (x = [a[0] - arm_slot, a[1]]) translate([x, y_top - 0.01, arm_z0]) cube([arm_slot, wall + 1, cap_z]);
            translate([a[0] - 0.01, y_top + arm_t, arm_z0]) cube([a[1] - a[0] + 0.02, wall, cap_z]);
        }
    }
    for (a = arms) hook(a[0], a[1]);
    for (e = eaves) eave(e[0], e[1]);
}

module deck() {
    color("#8fa9c4") difference() {
        union() {
            translate([0, 0, z_mid]) linear_extrude(mid_t) { cav2d(-deck_c); end2d(); }
            translate([0, 0, z_mid]) linear_extrude(cap_z - z_mid) end2d();
            for (p = pads) translate([p[0], p[1], z_up - 0.01]) cube([p[2] - p[0], p[3] - p[1], standoff + 0.01]);
            for (s = stops_top) translate([s[0], board[1] + gap, z_up - 0.01]) cube([s[1] - s[0], 1.5, z_top - 0.1 - z_up]);
            for (s = stops_right) translate([board[0] + gap, s[0], z_up - 0.01]) cube([1.5, s[1] - s[0], z_top - 0.1 - z_up]);
            button_seat();
        }
        // Notches for the keys.
        for (y = [y_bot - 1, y_top - key_p - deck_c])
            translate([keys[0] - deck_c, y, z_mid - 1]) cube([keys[1] - keys[0] + 2 * deck_c, key_p + deck_c + 1, mid_t + 2]);
        // USB and BAT windows (as case B).
        translate([lo[0] - 1, usb_center_y - usb_plug[0] / 2, z_top + usb_center_z - usb_plug[1] / 2]) cube([wall + 2, usb_plug[0], usb_plug[1]]);
        translate([lo[0] - 1, bat_span[0], z_top + 2.65 - 2.75]) cube([wall + 2, bat_span[1] - bat_span[0], 5.5]);
    }
}

// Case B's hook, on an arm whose inner face is the room's wall.
module hook(x0, x1) {
    tip = y_top - hook_p;
    under = catch_z + hook_p * tan(hook_under);
    color("#e8913a") along_x(x0, x1) polygon([
        [y_top + 0.01, catch_z], [y_top, catch_z], [tip, under], [tip, under + hook_tip],
        [y_top, under + hook_tip + hook_p], [y_top + 0.01, under + hook_tip + hook_p],
    ]);
}

module eave(x0, x1) {
    reach = eave_cover + lid_gap;
    color("#4caf50") along_x(x0, x1) polygon([
        [y_bot - 0.01, catch_z], [y_bot + 0.5, catch_z], [y_bot + reach, catch_z + reach - 0.5],
        [y_bot + reach, cap_z], [y_bot - 0.01, cap_z],
    ]);
}

// Posts under the button's corners, and L corners round its base.
module button_seat() {
    c = 2.6;
    s = 6 + btn_fit;
    translate([btn_c[0], btn_c[1], z_up - 0.01]) for (mx = [0, 1], my = [0, 1]) mirror([mx, 0, 0]) mirror([0, my, 0]) {
        translate([6 - c, 6 - c, 0]) cube([c, c, btn_z - z_up + 0.01]);
        translate([s, s - 2.2, 0]) cube([1.0, 3.2, btn_z - z_up + 2.0]);
        translate([s - 2.2, s, 0]) cube([3.2, 1.0, btn_z - z_up + 2.0]);
    }
}

module btn_cross(sq) {
    square(sq, center = true);
    square([btn_tab_slot, 15.6], center = true);
}

// In place: underside at lid_z, posts down to the board, a frame down onto
// the button's black base.
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
        }
        for (p = posts) translate([p[0], p[1], z_top + p[3] + 0.05]) cylinder(d = p[2], h = lid_z - z_top - p[3] - 0.05 + 0.01, $fn = 32);
    }
}

// In place, pushed home. Its floor sits on the bay's floor.
module tray() {
    y0 = bay[0][1] + tray_c;
    y1 = bay[1][1] - tray_c;
    x_face = bay[0][0];
    x_in = x_face + 1.6;                    // behind the face
    x_end = bay[1][0] - 0.2;
    x_arm1 = x_face + front;                // the arms' free ends
    color("#bcaaa4") translate([0, 0, z_bay]) {
        difference() {
            union() {
                // The face, shaped to the box's rounded corners.
                intersection() {
                    translate([x_face, y0, 0]) cube([1.6, y1 - y0, tray_h]);
                    translate([0, 0, -1]) linear_extrude(tray_h + 2) outer2d();
                }
                // A lip at its foot to pull it by.
                translate([x_face - 1.5, 20, 0]) cube([1.6, 10, 1.6]);
                // Floor: narrower between the arms, full width under the battery.
                translate([x_in - 0.01, y0 + arm_w + arm_room, 0]) cube([x_arm1 - x_in + 0.31, y1 - y0 - 2 * (arm_w + arm_room), tray_floor]);
                translate([x_arm1 + 0.3, y0, 0]) cube([x_end - x_arm1 - 0.3, y1 - y0, tray_floor]);
                // Stop rib in front of the battery: low, so the lead goes over it.
                translate([x_arm1 + 0.3, y0, 0]) cube([1.0, y1 - y0, tray_floor + 1.5]);
                // The arms, rooted on the face, each with a bump that clicks
                // into the bay's wall.
                for (side = [0, 1]) {
                    ya = side == 0 ? y0 : y1 - arm_w;
                    translate([x_in - 0.01, ya, 0]) cube([front - 1.6 + 0.01, arm_w, 5.0]);
                    translate([detent[0], side == 0 ? y0 : y1, 0]) linear_extrude(5.0)
                        polygon(side == 0
                            ? [[-detent[1] / 2, 0.01], [0, -detent[2]], [detent[1] / 2, 0.01]]
                            : [[-detent[1] / 2, -0.01], [0, detent[2]], [detent[1] / 2, -0.01]]);
                }
                // The 1000 mAh pouch's pocket, toward the FPC side; no higher
                // than the pouch, clear of the deck's ledges.
                if (battery == "1000") {
                    px1 = bat_x0 + bat[0] + 0.6;
                    py1 = y0 + bat[1] + 0.6;
                    translate([px1, y0, 0]) cube([1.2, py1 - y0 + 1.2, 7.0]);
                    translate([bat_x0, py1, 0]) cube([px1 - bat_x0 + 1.2, 1.2, 7.0]);
                }
            }
            // The lead's notch, open at the top.
            translate([x_face - 1, lead_slot[0], tray_h - 3.0]) cube([3.6, lead_slot[1] - lead_slot[0], 4]);
        }
    }
}

module pouch() {
    color("#d4e157") translate([bat_x0 + 0.3, bay[0][1] + tray_c + 0.3, z_bay + tray_floor]) rounded_box(bat, 1.5);
}

module button() {
    translate([btn_c[0], btn_c[1], btn_z]) {
        color("#222") translate([-6, -6, 0]) cube([12, 12, 3.8]);
        color("#eee") {
            translate([-3.95, -3.95, 3.8]) cube([7.9, 7.9, 2.7]);
            translate([-0.9, -7.55, 3.8]) cube([1.8, 15.1, 1.2]);
        }
        color("#fffde7") translate([0, 0, 7.1]) cylinder(d1 = 8.3, d2 = 7, h = 2.95, $fn = 48);
        color("#9e9e9e") {
            for (sx = [-1, 1], sy = [-1, 1]) translate([sx * 6.4 - 0.35, sy * 2.5 - 0.35, -4]) cube([0.7, 0.7, 4]);
            for (sy = [-1, 1]) translate([-0.25, sy * 7.0 - 0.25, -4]) cube([0.5, 0.5, 4 + 3.8]);
        }
    }
}

echo(str("case E: ", hi[0] - lo[0], " x ", hi[1] - lo[1], " x ", cap_z, " mm; button cap ", btn_proud,
         " mm out; under the button ", btn_z - z_up, " mm"));

part = "assembly";  // "tub", "deck", "lid", "tray" (laid out for printing), "assembly", "exploded", "none"
obj = "";
if (part == "tub") tub();
else if (part == "deck") translate([0, 0, -z_mid]) deck();
else if (part == "lid") translate([0, 0, lid_z + lid_t]) rotate([180, 0, 0]) lid();  // turned over, not mirrored
else if (part == "tray") translate([0, 0, -z_bay]) tray();
else if (part == "assembly" || part == "exploded") {
    e = part == "exploded";
    tub();
    translate([e ? -30 : 0, 0, e ? 12 : 0]) { deck(); translate([0, 0, z_top]) board_model(obj); button(); }
    translate([0, 0, e ? 25 : 0]) lid();
    translate([e ? -40 : 0, 0, 0]) { tray(); pouch(); }
}
