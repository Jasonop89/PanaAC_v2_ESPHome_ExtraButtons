/*
 * Copyright 2026 Hoang Minh
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "esphome/components/climate/climate_mode.h"
#include "esphome/core/log.h"
#include <cinttypes>
#include <cstdlib>
#include <cstring>

namespace esphome::panaac_v2 {

static const char *const TAG = "panaac_v2";

/// HVAC operation modes (mirrors climate::ClimateMode without pulling in the climate component).
enum Mode : uint8_t {
  MODE_OFF = 0,
  MODE_HEAT_COOL = 1,
  MODE_COOL = 2,
  MODE_HEAT = 3,
  MODE_FAN_ONLY = 4,
  MODE_DRY = 5,
  MODE_AUTO = 6,
};

// Temperature
static const uint8_t PANAAC_TEMP_MIN = 16;  // Celsius
static const uint8_t PANAAC_TEMP_MAX = 30;  // Celsius

constexpr float normalize_target_temperature(float value, float configured_step) {
  const float clamped = value < PANAAC_TEMP_MIN ? PANAAC_TEMP_MIN : (value > PANAAC_TEMP_MAX ? PANAAC_TEMP_MAX : value);
  const float protocol_step = configured_step >= 0.75f ? 1.0f : 0.5f;
  const auto increments = static_cast<uint8_t>(((clamped - PANAAC_TEMP_MIN) / protocol_step) + 0.5f);
  return PANAAC_TEMP_MIN + increments * protocol_step;
}

static_assert(normalize_target_temperature(15.0f, 0.5f) == 16.0f);
static_assert(normalize_target_temperature(23.2f, 0.5f) == 23.0f);
static_assert(normalize_target_temperature(23.3f, 0.5f) == 23.5f);
static_assert(normalize_target_temperature(23.4f, 1.0f) == 23.0f);
static_assert(normalize_target_temperature(23.5f, 1.0f) == 24.0f);
static_assert(normalize_target_temperature(31.0f, 1.0f) == 30.0f);

// Pulse parameters in usec
const uint16_t PANAAC_BIT_MARK = 550;
const uint16_t PANAAC_ONE_SPACE = 1200;
const uint16_t PANAAC_ZERO_SPACE = 350;
const uint16_t PANAAC_HEADER_MARK = 3650;
const uint16_t PANAAC_HEADER_SPACE = 1600;
const uint16_t PANAAC_FRAME_END = 10000;

// IR transmit frequency
const uint16_t PANAAC_IR_TRANSMIT_FREQ = 38000;

// byte position
const uint8_t PANAAC_BYTEPOS_POWER = 5;
const uint8_t PANAAC_BYTEPOS_MODE = 5;
const uint8_t PANAAC_BYTEPOS_TEMP = 6;
const uint8_t PANAAC_BYTEPOS_FAN = 8;
const uint8_t PANAAC_BYTEPOS_SWINGV = 8;
const uint8_t PANAAC_BYTEPOS_SWINGH = 9;
const uint8_t PANAAC_BYTEPOS_QUIET = 13;
const uint8_t PANAAC_BYTEPOS_POWERFUL = 13;
const uint8_t PANAAC_BYTEPOS_ECO = 17;
// nanoe-G: captured from a Panasonic inverter remote. It is a persistent state flag in byte 17
// bit 1: set on every following frame (e.g. after TEMP up) until nanoe-G is pressed again.
// The remote's normal frames also carry bit 7 (0x80) of byte 17; its timer/clock frames clear
// bit 7 and use bits 0-2 for the clock's high bits instead, so bit 7 marks "normal frame, nanoe
// flag valid". With supports_nanoe_g the encoder therefore always sends 0x80 (mirroring the remote).
const uint8_t PANAAC_BYTEPOS_NANOE_G = 17;
const uint8_t PANAAC_NANOE_FRAME_MARK = 0x80;

// Timers (captured from the same remote; see DESIGN.md "Timers"). Byte 5 bit 1 / bit 2 enable the ON / OFF
// timer; each timer is a 12-bit field (bit 11 always set, 0x600 = "none", otherwise minutes since
// 00:00): ON = byte10 | (byte11 & 0x0F) << 8, OFF = (byte11 >> 4) | byte12 << 4. Frames that carry timers
// also set byte 15 = 0x80 (normal frames: 0x89) and carry the remote's clock (minutes since 00:00) in
// byte 16 (low) and byte 17 bits 0-2 (high); byte 17 bit 7 is clear in those frames.
const uint8_t PANAAC_BYTEPOS_TIMER_ON = 10;
const uint8_t PANAAC_BYTEPOS_TIMER_MIX = 11;
const uint8_t PANAAC_BYTEPOS_TIMER_OFF = 12;
const uint8_t PANAAC_BYTEPOS_TIMER_MARK = 15;
const uint8_t PANAAC_BYTEPOS_CLOCK_LO = 16;
const uint8_t PANAAC_BYTEPOS_CLOCK_HI = 17;
const uint8_t PANAAC_TIMER_ON_FLAG = 0x02;
const uint8_t PANAAC_TIMER_OFF_FLAG = 0x04;
const uint8_t PANAAC_TIMER_FRAME_MARK = 0x80;
const uint16_t PANAAC_TIMER_FIELD_FLAG = 0x800;
const uint16_t PANAAC_TIMER_NONE = 0x600;
const uint16_t PANAAC_TIMER_MAX_MINUTES = 24 * 60;

// byte values
const uint8_t PANAAC_POWER_MASK = 0x01;  // only bit 0 encodes power state
const uint8_t PANAAC_POWERFUL = 0x01;
const uint8_t PANAAC_ECO = 0x10;
const uint8_t PANAAC_NANOE_G = 0x02;
const uint8_t PANAAC_POWER_OFF = 0x00;   // bit 0 = 0 -> OFF
const uint8_t PANAAC_POWER_ON = 0x01;    // bit 0 = 1 -> ON

const uint8_t PANAAC_MODE_DRY = 0x20;
const uint8_t PANAAC_MODE_COOL = 0x30;
const uint8_t PANAAC_MODE_HEAT = 0x40;
const uint8_t PANAAC_MODE_FAN_ONLY = 0x60;
const uint8_t PANAAC_MODE_AUTO = 0x00;

enum FanLevel : uint8_t {
  PANAAC_FAN_AUTO = 0xA0,
  PANAAC_FAN_LEVEL_1 = 0x30,
  PANAAC_FAN_LEVEL_2 = 0x40,
  PANAAC_FAN_LEVEL_3 = 0x50,
  PANAAC_FAN_LEVEL_4 = 0x60,
  PANAAC_FAN_LEVEL_5 = 0x70,
  PANAAC_FAN_QUIET = 0x20,
};

enum SwingVPos : uint8_t {
  PANAAC_SWINGV_AUTO = 0x0F,
  PANAAC_SWINGV_HIGHEST = 0x01,
  PANAAC_SWINGV_HIGH = 0x02,
  PANAAC_SWINGV_MIDDLE = 0x03,
  PANAAC_SWINGV_LOWEST = 0x05,
  PANAAC_SWINGV_LOW = 0x04,
};

enum SwingHPos : uint8_t {
  PANAAC_SWINGH_NONE = 0x00,
  PANAAC_SWINGH_MIDDLE = 0x06,
  PANAAC_SWINGH_LEFTMAX = 0x09,
  PANAAC_SWINGH_LEFT = 0x0A,
  PANAAC_SWINGH_RIGHT = 0x0B,
  PANAAC_SWINGH_RIGHTMAX = 0x0C,
  PANAAC_SWINGH_AUTO = 0x0D,
};

enum Preset : uint8_t {
  PANAAC_PRESET_NONE = 0,
  PANAAC_PRESET_POWERFUL = 1,
  PANAAC_PRESET_ECO = 2,
};

constexpr bool is_valid_swing_v_pos(uint8_t value) {
  return value == PANAAC_SWINGV_AUTO || value == PANAAC_SWINGV_HIGHEST || value == PANAAC_SWINGV_HIGH ||
         value == PANAAC_SWINGV_MIDDLE || value == PANAAC_SWINGV_LOW || value == PANAAC_SWINGV_LOWEST;
}

constexpr bool is_valid_swing_h_pos(uint8_t value) {
  return value == PANAAC_SWINGH_NONE || value == PANAAC_SWINGH_MIDDLE || value == PANAAC_SWINGH_LEFTMAX ||
         value == PANAAC_SWINGH_LEFT || value == PANAAC_SWINGH_RIGHT || value == PANAAC_SWINGH_RIGHTMAX ||
         value == PANAAC_SWINGH_AUTO;
}

static_assert(is_valid_swing_v_pos(PANAAC_SWINGV_AUTO));
static_assert(is_valid_swing_v_pos(PANAAC_SWINGV_LOWEST));
static_assert(!is_valid_swing_v_pos(0x07));
static_assert(is_valid_swing_h_pos(PANAAC_SWINGH_NONE));
static_assert(is_valid_swing_h_pos(PANAAC_SWINGH_RIGHTMAX));
static_assert(!is_valid_swing_h_pos(0x07));

/// Canonical Panasonic AC state — the single source of truth shared by the climate entity,
/// the 3 companion selects, the IR encoder/decoder, and (in v2 mode) the MQTT JSON publish.
/// Modeled on PanaAC v1's `ClimateState`. The Climate base fields (`mode`,
/// `target_temperature`, `fan_mode`/`custom_fan_mode`, `swing_mode`/custom swing strings) are
/// derived from this via `sync_to_climate_()`.
struct ClimateState {
  climate::ClimateMode mode;
  float temp;
  climate::ClimateFanMode fan_mode;
  FanLevel fan_level;
  climate::ClimateSwingMode swing_mode;
  SwingVPos swing_v_pos;
  SwingHPos swing_h_pos;
  SwingVPos last_swing_v_pos;
  SwingHPos last_swing_h_pos;
  Preset preset;
  bool nanoe_g;  // nanoe-G air purifier flag (only meaningful when supports_nanoe_g is set)
  // Timers (only meaningful when supports_timer is set). Minutes since 00:00, valid when enabled.
  bool on_timer_enabled;
  bool off_timer_enabled;
  uint16_t on_timer_minutes;
  uint16_t off_timer_minutes;
};

static const char *const STR_FAN_AUTO = "Auto";
static const char *const STR_FAN_L1 = "Level 1";
static const char *const STR_FAN_L2 = "Level 2";
static const char *const STR_FAN_L3 = "Level 3";
static const char *const STR_FAN_L4 = "Level 4";
static const char *const STR_FAN_L5 = "Level 5";
static const char *const STR_FAN_QUIET = "Quiet";
static const char *const STR_FAN_POWERFUL = "Powerful";

static const char *const STR_SWINGV_AUTO = "Auto";
static const char *const STR_SWINGV_HIGHEST = "Highest";
static const char *const STR_SWINGV_HIGH = "High";
static const char *const STR_SWINGV_MIDDLE = "Middle";
static const char *const STR_SWINGV_LOW = "Low";
static const char *const STR_SWINGV_LOWEST = "Lowest";

static const char *const STR_SWINGH_AUTO = "Auto";
static const char *const STR_SWINGH_LEFTMAX = "Left Max";
static const char *const STR_SWINGH_LEFT = "Left";
static const char *const STR_SWINGH_MIDDLE = "Middle";
static const char *const STR_SWINGH_RIGHT = "Right";
static const char *const STR_SWINGH_RIGHTMAX = "Right Max";

static const char *const STR_PRESET_NONE = "None";
static const char *const STR_PRESET_POWERFUL = "Powerful";
static const char *const STR_PRESET_ECO = "Eco";

inline const char *preset_to_str(Preset preset) {
  switch (preset) {
    case PANAAC_PRESET_POWERFUL:
      return STR_PRESET_POWERFUL;
    case PANAAC_PRESET_ECO:
      return STR_PRESET_ECO;
    default:
      return STR_PRESET_NONE;
  }
}

inline bool parse_preset(const char *value, Preset &preset) {
  if (strcmp(value, STR_PRESET_NONE) == 0 || strcmp(value, "none") == 0) {
    preset = PANAAC_PRESET_NONE;
  } else if (strcmp(value, STR_PRESET_POWERFUL) == 0 || strcmp(value, "powerful") == 0 ||
             strcmp(value, "boost") == 0 || strcmp(value, "BOOST") == 0) {
    preset = PANAAC_PRESET_POWERFUL;
  } else if (strcmp(value, STR_PRESET_ECO) == 0 || strcmp(value, "eco") == 0 || strcmp(value, "ECO") == 0) {
    preset = PANAAC_PRESET_ECO;
  } else {
    return false;
  }
  return true;
}

// Map a Panasonic fan-level byte to the user-facing string.
inline const char *fan_level_to_str(FanLevel level) {
  switch (level) {
    case PANAAC_FAN_LEVEL_1:
      return STR_FAN_L1;
    case PANAAC_FAN_LEVEL_2:
      return STR_FAN_L2;
    case PANAAC_FAN_LEVEL_3:
      return STR_FAN_L3;
    case PANAAC_FAN_LEVEL_4:
      return STR_FAN_L4;
    case PANAAC_FAN_LEVEL_5:
      return STR_FAN_L5;
    case PANAAC_FAN_QUIET:
      return STR_FAN_QUIET;
    default:
      return STR_FAN_AUTO;
  }
}

// Map a user-facing fan-mode string to the Panasonic fan-level byte.
inline bool parse_fan_level(const char *value, FanLevel &level) {
  if (strcmp(value, STR_FAN_AUTO) == 0)
    level = PANAAC_FAN_AUTO;
  else if (strcmp(value, STR_FAN_L1) == 0)
    level = PANAAC_FAN_LEVEL_1;
  else if (strcmp(value, STR_FAN_L2) == 0)
    level = PANAAC_FAN_LEVEL_2;
  else if (strcmp(value, STR_FAN_L3) == 0)
    level = PANAAC_FAN_LEVEL_3;
  else if (strcmp(value, STR_FAN_L4) == 0)
    level = PANAAC_FAN_LEVEL_4;
  else if (strcmp(value, STR_FAN_L5) == 0)
    level = PANAAC_FAN_LEVEL_5;
  else if (strcmp(value, STR_FAN_QUIET) == 0)
    level = PANAAC_FAN_QUIET;
  else
    return false;
  return true;
}

inline FanLevel fan_level_from_str(const char *value) {
  FanLevel level;
  return parse_fan_level(value, level) ? level : PANAAC_FAN_AUTO;
}
inline const char *swing_v_pos_to_str(SwingVPos pos) {
  switch (pos) {
    case PANAAC_SWINGV_HIGHEST:
      return STR_SWINGV_HIGHEST;
    case PANAAC_SWINGV_HIGH:
      return STR_SWINGV_HIGH;
    case PANAAC_SWINGV_MIDDLE:
      return STR_SWINGV_MIDDLE;
    case PANAAC_SWINGV_LOW:
      return STR_SWINGV_LOW;
    case PANAAC_SWINGV_LOWEST:
      return STR_SWINGV_LOWEST;
    default:
      return STR_SWINGV_AUTO;
  }
}

inline bool parse_swing_v_pos(const char *value, SwingVPos &pos) {
  if (strcmp(value, STR_SWINGV_HIGHEST) == 0)
    pos = PANAAC_SWINGV_HIGHEST;
  else if (strcmp(value, STR_SWINGV_HIGH) == 0)
    pos = PANAAC_SWINGV_HIGH;
  else if (strcmp(value, STR_SWINGV_MIDDLE) == 0)
    pos = PANAAC_SWINGV_MIDDLE;
  else if (strcmp(value, STR_SWINGV_LOW) == 0)
    pos = PANAAC_SWINGV_LOW;
  else if (strcmp(value, STR_SWINGV_LOWEST) == 0)
    pos = PANAAC_SWINGV_LOWEST;
  else if (strcmp(value, STR_SWINGV_AUTO) == 0)
    pos = PANAAC_SWINGV_AUTO;
  else
    return false;
  return true;
}

inline SwingVPos swing_v_pos_from_str(const char *value) {
  SwingVPos pos;
  return parse_swing_v_pos(value, pos) ? pos : PANAAC_SWINGV_AUTO;
}

inline const char *swing_h_pos_to_str(SwingHPos pos) {
  switch (pos) {
    case PANAAC_SWINGH_LEFTMAX:
      return STR_SWINGH_LEFTMAX;
    case PANAAC_SWINGH_LEFT:
      return STR_SWINGH_LEFT;
    case PANAAC_SWINGH_MIDDLE:
      return STR_SWINGH_MIDDLE;
    case PANAAC_SWINGH_RIGHT:
      return STR_SWINGH_RIGHT;
    case PANAAC_SWINGH_RIGHTMAX:
      return STR_SWINGH_RIGHTMAX;
    case PANAAC_SWINGH_AUTO:
      return STR_SWINGH_AUTO;
    default:
      return nullptr;  // not supported / none
  }
}

inline bool parse_swing_h_pos(const char *value, SwingHPos &pos) {
  if (strcmp(value, STR_SWINGH_LEFTMAX) == 0)
    pos = PANAAC_SWINGH_LEFTMAX;
  else if (strcmp(value, STR_SWINGH_LEFT) == 0)
    pos = PANAAC_SWINGH_LEFT;
  else if (strcmp(value, STR_SWINGH_MIDDLE) == 0)
    pos = PANAAC_SWINGH_MIDDLE;
  else if (strcmp(value, STR_SWINGH_RIGHT) == 0)
    pos = PANAAC_SWINGH_RIGHT;
  else if (strcmp(value, STR_SWINGH_RIGHTMAX) == 0)
    pos = PANAAC_SWINGH_RIGHTMAX;
  else if (strcmp(value, STR_SWINGH_AUTO) == 0)
    pos = PANAAC_SWINGH_AUTO;
  else
    return false;
  return true;
}

inline SwingHPos swing_h_pos_from_str(const char *value) {
  SwingHPos pos;
  return parse_swing_h_pos(value, pos) ? pos : PANAAC_SWINGH_NONE;
}

inline const char *mode_to_str(Mode mode) {
  switch (mode) {
    case MODE_COOL:
      return "cool";
    case MODE_HEAT:
      return "heat";
    case MODE_FAN_ONLY:
      return "fan_only";
    case MODE_DRY:
      return "dry";
    case MODE_AUTO:
      return "auto";
    default:
      return "off";
  }
}

inline Mode mode_from_str(const char *value) {
  if (strcasecmp(value, "cool") == 0)
    return MODE_COOL;
  if (strcasecmp(value, "heat") == 0)
    return MODE_HEAT;
  if (strcasecmp(value, "fan_only") == 0)
    return MODE_FAN_ONLY;
  if (strcasecmp(value, "dry") == 0)
    return MODE_DRY;
  if (strcasecmp(value, "auto") == 0)
    return MODE_AUTO;
  return MODE_OFF;
}

// PanaAC Mode enum values are intentionally identical to climate::ClimateMode.
inline climate::ClimateMode mode_to_climate_mode(Mode mode) {
  return static_cast<climate::ClimateMode>(mode);
}
inline Mode climate_mode_to_mode(climate::ClimateMode mode) {
  return static_cast<Mode>(mode);
}

}  // namespace esphome::panaac_v2
