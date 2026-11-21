// The LED button (Omten TSD1265) in its own small box, wired to case B's
// EXT window, taped wherever it's handy (case B's lid, the frame).
//
// Two parts. The box's top rests on the black base, so everything white
// stands out through it: the cap sits about 4 mm proud. The base plate
// snaps in from below (two clips a side, into windows in the box's walls;
// push the hooks in through the windows to take it out again) and holds the
// body up on four corner posts, so a press goes straight into whatever the
// box is taped to.
//
// Solder the wires first, with their connector already on, then put the
// button in. The wires leave by a notch in one wall that's open at the
// bottom: lay them in from below and snap the plate on, the connector never
// goes through anything. Cut the legs to about 4 mm.
//
// Print the box top down (part = "box" lays it out that way) and the plate
// flat, both without supports.
//
//   openscad -o button_box.stl   -D 'part="box"'   button_box.scad
//   openscad -o button_plate.stl -D 'part="plate"' button_box.scad

// From the TSD1265 drawing. The LED tabs stick out along Y from the white
// part, over the black base; the switch legs come out the X sides, 5
// apart, and bend out to 13.5 across.
btn_w = 12;
btn_base_h = 3.8;
btn_shoulder_h = 6.5;  // body bottom to the white part's fixed square top
btn_shoulder_w = 7.9;
btn_h = 10.05;         // body bottom to the cap's top
btn_cap_d = 8.3;
btn_tabs = [15.1, 1.8];

fit = 0.15;      // around the body
under = 5.0;     // under the body: cut legs, solder, wires, the clips
top_t = 2.0;
wall = 1.6;
plate_t = 1.2;
room = 15.6;     // under the body: the bent legs and the LED legs
r = 2.5;         // outside corners
hole = 9.0;      // the top's square hole, round the white part
tab_slot = 2.6;  // for the LED tabs and their legs, along Y
notch = [6, 2.8];  // the wires' way out, over the plate: wide, tall

// The clips: on the ±Y sides, clear of the LED legs (x = 0) and the posts.
clip_x = [1.4, 5.4];  // from, to (and mirrored)
clip_t = 0.8;
hook = 0.5;           // how far the hook stands out of the clip
hook_z = 5.0;         // its flat, catching face
plate_c = 0.1;        // around the plate

body_z = plate_t + under;
top_z = body_z + btn_base_h + 0.05;  // the top's underside, on the black base
h = top_z + top_t;
proud = body_z + btn_h - h;
outer = room + 2 * wall;
plate_w = room - 2 * plate_c;

module rsquare(s, rr) { offset(r = rr) offset(delta = -rr) square(s, center = true); }

module cross(sq) {
    square(sq, center = true);
    square([tab_slot, room], center = true);
}

module box() {
    color("#78909c") difference() {
        linear_extrude(h) rsquare(outer, r);
        translate([0, 0, -1]) linear_extrude(body_z + 1) square(room, center = true);
        translate([0, 0, body_z - 0.01]) linear_extrude(top_z - body_z + 0.02) cross(btn_w + 2 * fit);
        translate([0, 0, top_z - 0.01]) linear_extrude(top_t + 1) cross(hole);
        // The wires' way out, open at the bottom.
        translate([-outer / 2 - 1, -notch[0] / 2, -1]) cube([wall + 2, notch[0], plate_t + notch[1] + 1]);
        // Windows the clips' hooks catch in.
        for (s = [-1, 1], sx = [-1, 1])
            translate([sx > 0 ? clip_x[0] - 0.2 : -clip_x[1] - 0.2, s > 0 ? room / 2 - 0.5 : -outer / 2 - 0.5, hook_z - 0.05])
                cube([clip_x[1] - clip_x[0] + 0.4, wall + 1, 1.0]);
    }
}

module plate() {
    c = 2.6;
    color("#b0bec5") {
        translate([0, 0, 0]) linear_extrude(plate_t) square(plate_w, center = true);
        // Posts under the body's corners, clear of the legs (mid X sides)
        // and the LED legs (mid Y sides).
        for (sx = [-1, 1], sy = [-1, 1])
            translate([sx * (btn_w / 2 - c / 2) - c / 2, sy * (btn_w / 2 - c / 2) - c / 2, plate_t - 0.01]) cube([c, c, under + 0.01]);
        // The clips, flush with the plate's edge, and their hooks: a flat
        // face underneath to catch, a ramp on top to go in.
        for (s = [-1, 1], sx = [-1, 1]) mirror([0, s < 0 ? 1 : 0, 0])
            translate([sx > 0 ? clip_x[0] : -clip_x[1], 0, 0]) {
                translate([0, plate_w / 2 - clip_t, plate_t - 0.01]) cube([clip_x[1] - clip_x[0], clip_t, hook_z + 0.9 - plate_t + 0.01]);
                rotate([90, 0, 90]) linear_extrude(clip_x[1] - clip_x[0])
                    polygon([[plate_w / 2 - 0.01, hook_z], [plate_w / 2 + hook, hook_z], [plate_w / 2 - 0.01, hook_z + 0.9]]);
            }
    }
}

module button() {
    translate([0, 0, body_z]) {
        color("#222") translate([-btn_w / 2, -btn_w / 2, 0]) cube([btn_w, btn_w, btn_base_h]);
        color("#eee") {
            translate([-btn_shoulder_w / 2, -btn_shoulder_w / 2, btn_base_h]) cube([btn_shoulder_w, btn_shoulder_w, btn_shoulder_h - btn_base_h]);
            translate([-btn_tabs[1] / 2, -btn_tabs[0] / 2, btn_base_h]) cube([btn_tabs[1], btn_tabs[0], 1.5]);
        }
        color("#9e9e9e") {
            for (sx = [-1, 1], sy = [-1, 1]) translate([sx * 6.4 - 0.35, sy * 2.5 - 0.35, -4]) cube([0.7, 0.7, 4]);
            for (sy = [-1, 1]) translate([-0.25, sy * 7.0 - 0.25, -4]) cube([0.5, 0.5, 4 + btn_base_h]);
        }
        color("#fffde7") translate([0, 0, btn_h - 2.95]) cylinder(d1 = btn_cap_d, d2 = 7, h = 2.95, $fn = 48);
    }
}

echo(str("button box: ", outer, " x ", outer, " x ", h, " mm, cap ", proud, " out of the top"));

part = "assembly";  // "box", "plate", "assembly", "exploded", "none"
if (part == "box") translate([0, 0, h]) rotate([180, 0, 0]) box();  // turned over, not mirrored
else if (part == "plate") plate();
else if (part == "assembly") { box(); plate(); button(); }
else if (part == "exploded") { translate([0, 0, 22]) box(); translate([0, 0, 10]) button(); plate(); }
