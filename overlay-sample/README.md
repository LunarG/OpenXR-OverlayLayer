# Sample Overlay app for v2 of the api

This is a overlay sample that is meant to run as similarly as possible in a role as main app or as an overlay app as well as do some basic input processing.

Current behavior is the same as OverlayUser_Remote aside from taking the aim pose and select actions of the left and right hand to determine intersection with the overlay quad slide show.

Intersection is determined as though the quad was a circle with diameter of the shortest extent.

If intersection with the quad is detected, the image will become an obnoxous purple color. Further, if the select action is then triggerd a sticky yellow version of the image will be toggled and normal behavior will resume if it is toggled off during an intersesction.

This is a very rough means to test out actions and actionSets with the overlay extension.