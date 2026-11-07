# Shutter

A shutter for Tessera screens, drawn like Home Assistant's tile card with its cover features: the icon, the name and
the position on top, a row of keys (up, stop, down) and a row of favourite positions under it. The position the shutter
stands at is in the accent; a key that has nothing to do (up when it is open, down when it is closed) is grey.

A tile too low for three rows puts the keys beside the name and keeps the favourites where one row fits, like the small
rows of Home Assistant's dashboard. Favourites that do not fit the width are left out at the end.

## Set up

1. In Tessera, open Plugins, choose **Add with a link** and paste the link of this repository.
2. Add the plugin to a screen and let it build.
3. In Layout, place **Shutter**, choose its cover in the inspector and drag it to the size you want: 4x2 or larger for
   all three rows.
4. Under **Favourites**, list the positions in percent (100 = open), in the order of the keys. Each tile has its own.
5. The screen must be allowed to perform actions (Screen settings). The tile calls `cover.open_cover`,
   `cover.close_cover`, `cover.stop_cover` and `cover.set_cover_position`.

## License

MIT
