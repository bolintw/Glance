// A drawer for the LiPo pouch, under case B. Stacked from the frame up:
// drawer, then case B taped on its top. The pouch slides in from the USB
// side and a plug closes the end; nothing is glued to the pouch. Its lead
// is laid into a notch in the plug's corner (open to the side, so the
// connector stays outside) and goes up, outside, into case B's battery
// window. Lay a strip of ribbon under the pouch, sticking out of
// the end, to pull it back out by.
//
// The pouch stays clear of the ESP32's antenna (it's foil): its edge stops
// short of the antenna, toward the FPC side, even if the drawer then sticks
// out past case B there (the 2000 mAh one does).
//
// Print the drawer lying on its side (part = "drawer" lays it out that way:
// the walls and its top stand up, and the only bridge is across the pouch's
// thickness) and the plug face down, both without supports.
//
//   openscad -o drawer.stl -D 'part="drawer"' -D 'battery="1000"' battery_drawer.scad
//   openscad -o plug.stl   -D 'part="plug"'   -D 'battery="1000"' battery_drawer.scad

include <glance_v31.scad>

battery = "1000";  // "1000" (52 x 34 x 6) or "2000" (70 x 50 x 5), measured
bat = battery == "2000" ? [70, 50, 5] : [52, 34, 6];
room = [1.0, 0.8, 0.6];  // over the pouch: length, width, thickness

// Case B's outside, in board coordinates: its cavity (-0.3..58.3,
// -0.3..46.3) grown by its 2.0 wall, corners r 5.3.
b_lo = [-2.3, -2.3];
b_hi = [60.3, 48.3];
b_r = 5.3;

wall = 1.6;
floor_t = 1.2;
top_t = 1.6;
plug_t = 1.6;       // the plug's face
plug_d = 3.0;       // how far it goes in
pouch_max_y = 38;   // the module's antenna starts at about y = 40

inner = bat + room;
h = floor_t + inner[2] + top_t;
// The pouch's room, in board coordinates.
iy0 = min(b_lo[1] + wall + 0.7, pouch_max_y - inner[1]);
iy1 = iy0 + inner[1];
ix0 = b_lo[0] + plug_t;         // the open end; the plug's face is flush with case B
ix1 = ix0 + plug_d + inner[0];
iz0 = floor_t;
iz1 = floor_t + inner[2];
// The tube's sides; the -Y one reaches as far as the top, so the drawer
// lies flat on that side for printing.
ty0 = min(iy0 - wall, b_lo[1]);
ty1 = iy1 + wall;

module rounded_rect(lo, hi, r) {
    translate(lo) offset(r = r) offset(delta = -r) square(hi - lo);
}

module drawer() {
    color("#8d6e63") difference() {
        union() {
            // The tube.
            translate([ix0, ty0, 0]) cube([ix1 + wall - ix0, ty1 - ty0, h]);
            // Its top, wide enough for case B to sit on.
            translate([0, 0, h - top_t]) linear_extrude(top_t) union() {
                rounded_rect(b_lo, b_hi, b_r);
                translate([ix0, ty0]) square([ix1 + wall - ix0, ty1 - ty0]);
            }
        }
        translate([ix0 - 1, iy0, iz0]) cube([ix1 - ix0 + 1, inner[1], inner[2]]);
    }
}

// The plug: a face over the tube's end and a hollow frame that goes in.
// The lead runs across inside the frame, wherever it leaves the pouch, and
// out through a notch in the +Y corner, under case B's battery window. The
// notch is open to the side, so the lead is laid in from there: the
// connector never has to go through anything.
module plug() {
    c = 0.1;  // clearance; the ribs make up for it
    notch = [4.2, 2.8];  // how far in from the +Y side, and how tall
    color("#bcaaa4") difference() {
        union() {
            translate([b_lo[0], ty0, 0]) cube([plug_t, ty1 - ty0, h - top_t]);
            translate([ix0 - 0.01, iy0 + c, iz0 + c]) cube([plug_d + 0.01, inner[1] - 2 * c, inner[2] - 2 * c]);
            // Crush ribs on the sides, for a snug fit.
            for (y = [iy0 + c - 0.25, iy1 - c - 0.01], z = [iz0 + 1, iz1 - 1.6])
                translate([ix0, y, z]) cube([plug_d, 0.26, 0.6]);
        }
        translate([b_lo[0] - 1, ty1 - notch[0], (iz0 + iz1) / 2 - notch[1] / 2]) cube([plug_t + plug_d + 2, notch[0] + 1, notch[1]]);
        // Hollow, for the lead and so the frame gives a little.
        translate([ix0 + 0.8, iy0 + 1.6, iz0 + c - 1]) cube([plug_d, inner[1] - 3.2, inner[2] + 2]);
    }
}

module pouch() {
    color("#d4e157") translate([ix0 + plug_d + room[0] / 2, iy0 + room[1] / 2, iz0 + room[2] / 2]) rounded_box(bat, 1.5);
}

echo(str("battery drawer ", battery, "mAh: tube ", ix1 + wall - ix0, " x ", ty1 - ty0, " x ", h,
         " mm, pouch y ", iy0 + room[1] / 2, "..", iy1 - room[1] / 2, " (antenna from ~40)"));

part = "assembly";  // "drawer", "plug", "assembly", "none"
// rotate +90 about X takes Y to Z: the tube's -Y side goes down.
if (part == "drawer") translate([0, 0, -ty0]) rotate([90, 0, 0]) drawer();
// rotate -90 about Y takes X to Z: the plug's face goes down.
else if (part == "plug") translate([0, 0, -b_lo[0]]) rotate([0, -90, 0]) plug();
else if (part == "assembly") { drawer(); plug(); pouch(); }
