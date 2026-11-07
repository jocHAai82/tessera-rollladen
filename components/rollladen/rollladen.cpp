#include "rollladen.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace esphome::rollladen {

using tessera::Font;
namespace ui = tessera::ui;

// Icons of Tessera's set (screen_manager/app/tile_icons.py).
constexpr uint32_t ICON_CLOSED = 0xF111C;  // window-shutter
constexpr uint32_t ICON_OPEN = 0xF111E;    // window-shutter-open
constexpr uint32_t ICON_UP = 0xF005D;      // arrow-up
constexpr uint32_t ICON_STOP = 0xF04DB;    // stop
constexpr uint32_t ICON_DOWN = 0xF0045;    // arrow-down

constexpr int MAX_FAVS = 12;
enum Key : uint8_t { UP, STOP, DOWN, KEYS };

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  bool has(int px, int py) const { return w > 0 && px >= x && px < x + w && py >= y && py < y + h; }
};

// A shutter as Home Assistant's tile card draws it with its features: the icon, the name and "Open · 8 %" on top, a
// row of keys (up, stop, down) and a row of favourite positions under it, the one it stands at in the accent. A tile
// too low for all three rows puts the keys beside the name, and leaves out the favourites where there is no room.
class CoverTile : public tessera::Tile {
 public:
  explicit CoverTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    parent_ = c.parent;
    width_ = c.width;
    height_ = c.height;
    entity_ = c.entity ? c.entity : "";
    if (c.name && *c.name) own_name_ = c.name;

    // The favourites: whole numbers from 0 to 100, in the order the person put them.
    JsonArrayConst favs = c.options["favorites"].as<JsonArrayConst>();
    for (JsonVariantConst f : favs) {
      if (fav_count_ >= MAX_FAVS) break;
      const char *t = f.as<const char *>();
      if (!t) continue;
      char *end = nullptr;
      const long v = strtol(t, &end, 10);
      if (end == t || v < 0 || v > 100) continue;
      fav_[fav_count_++] = int(v);
    }
    const bool keys = c.options["keys"] | true;

    const int g = ui::px(10);
    const int hb = ui::mm(9) > ui::line_height(Font::BODY_LARGE) + ui::px(16) ? ui::mm(9)
                                                                               : ui::line_height(Font::BODY_LARGE) + ui::px(16);
    const int ic = ui::px(44);
    const int ht = ui::line_height(Font::TITLE), hs = ui::line_height(Font::BODY);
    const int hh = ic > ht + hs ? ic : ht + hs;

    // Which rows fit: the keys and the favourites each under the header; else the keys beside the name, and the
    // favourites under it where one row fits.
    const int kw = hb + ui::px(10);  // a key beside the name: a little wider than high
    const bool inline_room = width_ >= ic + ui::px(100) + 3 * kw + 3 * ui::px(8);
    bool keys_row = false, keys_inline = false, favs_row = false;
    if (keys && fav_count_ > 0 && height_ >= hh + 2 * (g + hb)) {
      keys_row = favs_row = true;
    } else if (keys && fav_count_ > 0 && height_ >= hh + g + hb && inline_room) {
      keys_inline = favs_row = true;
    } else if (keys && height_ >= hh + g + hb && !inline_room) {
      keys_row = true;
    } else if (keys && inline_room) {
      keys_inline = true;
    } else if (!keys && fav_count_ > 0 && height_ >= hh + g + hb) {
      favs_row = true;
    }
    const int total = hh + (keys_row ? g + hb : 0) + (favs_row ? g + hb : 0);
    int y = (height_ - total) / 2;
    if (y < 0) y = 0;

    // The header: the icon in its circle, the name, the state and the position.
    icon_bg_ = ui::block(parent_, theme::TRACK);
    lv_obj_set_size(icon_bg_, ic, ic);
    lv_obj_set_style_radius(icon_bg_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_pos(icon_bg_, 0, y + (hh - ic) / 2);
    icon_ = ui::label(parent_, Font::ICON_SMALL, theme::MUTED);
    lv_obj_set_width(icon_, ic);
    lv_obj_set_style_text_align(icon_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(icon_, 0, y + (hh - ui::line_height(Font::ICON_SMALL)) / 2);
    ui::set_text(icon_, ui::icon(ICON_CLOSED));

    const int tx = ic + ui::px(12);
    const int text_right = keys_inline ? width_ - 3 * kw - 3 * ui::px(8) : width_;
    name_ = ui::label(parent_, Font::TITLE, theme::INK);
    state_ = ui::label(parent_, Font::BODY, theme::MUTED);
    for (lv_obj_t *l : {name_, state_}) lv_obj_set_width(l, text_right - tx > 0 ? text_right - tx : 0);
    lv_obj_set_pos(name_, tx, y + (hh - ht - hs) / 2);
    lv_obj_set_pos(state_, tx, y + (hh - ht - hs) / 2 + ht);

    // The keys: in a row of their own across the tile, or beside the name.
    if (keys_row || keys_inline) {
      for (int k = 0; k < KEYS; ++k) {
        Rect r;
        if (keys_row) {
          r.w = (width_ - 2 * ui::px(12)) / 3;
          r.h = hb;
          r.x = k * (r.w + ui::px(12));
          r.y = y + hh + g;
        } else {
          r.w = kw;
          r.h = hb;
          r.x = width_ - (KEYS - k) * (kw + ui::px(8)) + ui::px(8);
          r.y = y + (hh - hb) / 2;
        }
        key_rect_[k] = r;
        key_[k] = ui::block(parent_, theme::BUTTON);
        lv_obj_set_size(key_[k], r.w, r.h);
        lv_obj_set_pos(key_[k], r.x, r.y);
        key_icon_[k] = ui::label(key_[k], Font::ICON_SMALL, theme::INK);
        lv_obj_set_width(key_icon_[k], r.w);
        lv_obj_set_style_text_align(key_icon_[k], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(key_icon_[k], 0, (r.h - ui::line_height(Font::ICON_SMALL)) / 2);
        ui::set_text(key_icon_[k], ui::icon(k == UP ? ICON_UP : k == STOP ? ICON_STOP : ICON_DOWN));
      }
    }

    // The favourites: one track across the tile, each position an equal part of it, as many as fit.
    if (favs_row) {
      const int ry = y + hh + g + (keys_row ? hb + g : 0);
      row_ = ui::block(parent_, theme::BUTTON);
      lv_obj_set_size(row_, width_, hb);
      lv_obj_set_pos(row_, 0, ry);
      const int least = ui::text_width("100%", Font::BODY_LARGE) + ui::px(12);
      int fit = width_ / (least > 0 ? least : 1);
      if (fit > fav_count_) fit = fav_count_;
      shown_favs_ = fit;
      for (int i = 0; i < shown_favs_; ++i) {
        Rect r;
        r.x = i * width_ / shown_favs_;
        r.w = (i + 1) * width_ / shown_favs_ - r.x;
        r.y = ry;
        r.h = hb;
        chip_rect_[i] = r;
        chip_[i] = ui::block(parent_, theme::ACCENT);
        lv_obj_set_size(chip_[i], r.w, r.h);
        lv_obj_set_pos(chip_[i], r.x, r.y);
        lv_obj_add_flag(chip_[i], LV_OBJ_FLAG_HIDDEN);
        chip_text_[i] = ui::label(parent_, Font::BODY_LARGE, theme::INK);
        lv_obj_set_width(chip_text_[i], r.w);
        lv_obj_set_style_text_align(chip_text_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(chip_text_[i], r.x, r.y + (r.h - ui::line_height(Font::BODY_LARGE)) / 2);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", fav_[i]);
        ui::set_text(chip_text_[i], buf);
      }
    }
  }

  void on_state(JsonObjectConst data) override {
    has_state_ = data["state"].is<const char *>();
    state_word_ = has_state_ ? data["state"].as<const char *>() : "";
    if (data["name"].is<const char *>()) ha_name_ = data["name"].as<const char *>();
    JsonVariantConst p = data["attributes"]["current_position"];
    position_ = p.is<int>() || p.is<float>() ? p.as<int>() : -1;
  }

  void on_tick(uint32_t) override {
    if (entity_.empty() && !has_state_) {
      ui::set_text(name_, plugin_->text("choose"));
      ui::set_text(state_, "");
      return;
    }
    ui::set_text(name_, own_name_.empty() ? ha_name_ : own_name_);

    // "Open · 8 %", as Home Assistant says it.
    const bool unavailable = state_word_ == "unavailable" || state_word_ == "unknown";
    std::string words;
    if (unavailable) words = plugin_->text("unavailable");
    else if (state_word_ == "opening" || state_word_ == "closing" || state_word_ == "closed" || state_word_ == "open")
      words = plugin_->text(state_word_.c_str());
    if (!unavailable && position_ >= 0) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%s%d %%", words.empty() ? "" : " · ", position_);
      words += buf;
    }
    ui::set_text(state_, words);

    // Open in the accent, closed grey: the icon and its circle.
    const bool open = !unavailable && (position_ > 0 || (position_ < 0 && state_word_ != "closed"));
    ui::set_text(icon_, ui::icon(open ? ICON_OPEN : ICON_CLOSED));
    ui::set_color(icon_, open ? theme::ACCENT : theme::MUTED);
    set_fill(icon_bg_, open ? theme::ACCENT_TINT : theme::TRACK);

    // A key that has nothing to do is grey: up when the shutter is open, down when it is closed.
    up_off_ = unavailable || position_ == 100 || (position_ < 0 && state_word_ == "open");
    down_off_ = unavailable || position_ == 0 || (position_ < 0 && state_word_ == "closed");
    if (key_icon_[UP]) ui::set_color(key_icon_[UP], up_off_ ? theme::SUBTLE : theme::INK);
    if (key_icon_[STOP]) ui::set_color(key_icon_[STOP], unavailable ? theme::SUBTLE : theme::INK);
    if (key_icon_[DOWN]) ui::set_color(key_icon_[DOWN], down_off_ ? theme::SUBTLE : theme::INK);

    // The favourite it stands at, in the accent.
    for (int i = 0; i < shown_favs_; ++i) {
      const bool here = !unavailable && position_ == fav_[i];
      set_hidden(chip_[i], !here);
      ui::set_color(chip_text_[i], here ? theme::ON_ACCENT : theme::INK);
    }
    unavailable_ = unavailable;
  }

  void on_theme() override {
    for (lv_obj_t *k : key_) if (k) lv_obj_set_style_bg_color(k, ui::color(theme::BUTTON), 0);
    if (row_) lv_obj_set_style_bg_color(row_, ui::color(theme::BUTTON), 0);
    for (int i = 0; i < shown_favs_; ++i) lv_obj_set_style_bg_color(chip_[i], ui::color(theme::ACCENT), 0);
    fill_ = -1;
    on_tick(0);
  }

  void on_tap() override {
    if (entity_.empty() || unavailable_) return;
    const tessera::Point p = tessera::tap_point();
    if (p.x < 0) return;
    if (key_rect_[UP].has(p.x, p.y)) {
      if (!up_off_) tessera::action("cover.open_cover", entity_);
      return;
    }
    if (key_rect_[STOP].has(p.x, p.y)) {
      tessera::action("cover.stop_cover", entity_);
      return;
    }
    if (key_rect_[DOWN].has(p.x, p.y)) {
      if (!down_off_) tessera::action("cover.close_cover", entity_);
      return;
    }
    for (int i = 0; i < shown_favs_; ++i) {
      if (!chip_rect_[i].has(p.x, p.y)) continue;
      JsonDocument d;
      d["position"] = fav_[i];
      tessera::action("cover.set_cover_position", entity_, d.as<JsonVariantConst>());
      return;
    }
  }

 private:
  void set_fill(lv_obj_t *block, theme::Role role) {
    if (int(role) == fill_) return;
    fill_ = int(role);
    lv_obj_set_style_bg_color(block, ui::color(role), 0);
  }

  static void set_hidden(lv_obj_t *o, bool hidden) {
    if (!o) return;
    if (hidden != lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) {
      if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
  }

  const tessera::Plugin *plugin_;
  lv_obj_t *parent_{};
  int width_ = 0, height_ = 0;
  std::string entity_, own_name_, ha_name_, state_word_;
  bool has_state_ = false, unavailable_ = false, up_off_ = false, down_off_ = false;
  int position_ = -1;
  int fav_[MAX_FAVS]{};
  int fav_count_ = 0, shown_favs_ = 0;
  int fill_ = -1;
  lv_obj_t *icon_bg_{}, *icon_{}, *name_{}, *state_{}, *row_{};
  lv_obj_t *key_[KEYS]{}, *key_icon_[KEYS]{};
  Rect key_rect_[KEYS];
  lv_obj_t *chip_[MAX_FAVS]{}, *chip_text_[MAX_FAVS]{};
  Rect chip_rect_[MAX_FAVS];
};

void Rollladen::setup() {
  add_tile("cover", [this]() { return new CoverTile(this); });
}

}  // namespace esphome::rollladen
