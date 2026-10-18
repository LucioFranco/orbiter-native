# External input diagnostic

These SDK module/adapter sources stay outside production CMake targets.
The native adapter requires StartPaused FALSE before Launch and rejects TRUE
before loading a module/world. Stage fresh runtimes with
tools/stage_nassp_manual.py; the adapter owns every step, including the first.
It checks positive actual time advance per step and records actual_sim_time_s.
Do not resume a TRUE-configured host to align this protocol: that would add an
unwanted step. Pulse steps remain302–319, with release before320.
Historical binary/evidence hashes remain unchanged; rebuild this updated
external adapter separately for new captures.
