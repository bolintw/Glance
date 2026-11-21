# Case drafts

Every case tried for the V3.1 board, in OpenSCAD. The one in use is
[full_closed](../full_closed/); these are the steps on the way there, kept
for reference.

| File | Case | Status |
|---|---|---|
| `glance_v31.scad` | The board's outline and parts, shared by every case | |
| `case_in_frame.scad` | A: an open tray for the board, inside the photo frame | Board only |
| `case_back_mount.scad` | B: a closed box for the board, on the back of the frame | Printed and fitted (2026-11-10); F grew out of it |
| `battery_drawer.scad` | D: a battery drawer under B | Replaced by E |
| `button_box.scad` | D: the LED button in a box of its own | Replaced by E |
| `case_one.scad` | E: two floors, the board over the battery | Dropped: the battery lifts the board past the FPC's reach |
| `case_flat.scad` | F: board, battery and button side by side | Printed as [full_closed](../full_closed/); this copy has changes not yet printed (open floor under the button, more lid posts over the battery, a finger hole in the lid) |
| `case_in_frame_battery.scad` | H: case A's open tray with a battery bay beside it, for inside the frame; two straps hold the pouch, the button plugs into EXT from outside | Not printed yet (2026-11-21) |

Each file's header says how it goes together and how to export its STLs.
