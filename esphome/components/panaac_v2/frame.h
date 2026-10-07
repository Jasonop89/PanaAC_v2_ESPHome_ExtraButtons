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

#include "definitions.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::panaac_v2 {

/// Fixed first frame that precedes every Panasonic AC command.
static constexpr std::array<uint8_t, 8> PANAAC_FIRST_FRAME = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06};

/// SLEEP button, captured from a Panasonic inverter remote. It is not part of the 19-byte state
/// frame: the remote sends the usual fixed first frame followed by a short 8-byte frame
/// `02 20 E0 04 80 <step> 10 <checksum>` (checksum = low byte of the sum of the first 7 bytes).
/// Each press of SLEEP advances to the next value (11 presses were captured, in this order; the
/// last, 0x00, is the end of the cycle). What each step means on the AC is not known.
static constexpr std::array<uint8_t, 11> PANAAC_SLEEP_STEPS = {0x1F, 0x1E, 0x1C, 0x1A, 0x18, 0x16,
                                                               0x14, 0x12, 0x10, 0x0E, 0x00};

inline std::array<uint8_t, 8> build_sleep_frame(size_t step) {
  std::array<uint8_t, 8> frame = {0x02, 0x20, 0xE0, 0x04, 0x80, PANAAC_SLEEP_STEPS[step % PANAAC_SLEEP_STEPS.size()],
                                  0x10, 0x00};
  for (size_t i = 0; i < 7; i++)
    frame[7] = static_cast<uint8_t>(frame[7] + frame[i]);
  return frame;
}

/// Capability flags that influence which bits are written into the state frame.
struct FrameCaps {
  bool supports_quiet{false};
  bool supports_powerful{false};
  bool supports_eco{false};
  bool supports_nanoe_g{false};
  bool swing_horizontal{false};
};

/// When `include` is set the frame uses the remote's timer layout: timer fields, byte 15 = 0x80 and
/// the clock (minutes since 00:00) in bytes 16-17. Frames with the timers disabled but `include`
/// set are what cancels the timers on the AC. Timer frames carry no nanoe-G flag (the clock's high
/// bits share that byte).
struct TimerFrameInfo {
  bool include{false};
  uint16_t clock_minutes{0};
};

/// Build the variable 19-byte second frame (including checksum) for `state`.
/// Pure function: no ESPHome runtime dependencies, so it can be unit-tested on the host.
inline std::array<uint8_t, 19> build_state_frame(const ClimateState &state, const FrameCaps &caps,
                                                 const TimerFrameInfo &timer = TimerFrameInfo{}) {
  std::array<uint8_t, 19> second_frame = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
                                          0x00, 0x0E, 0xE0, 0x00, 0x00, 0x89, 0x00, 0x00, 0x00};

  // power & mode
  switch (state.mode) {
    case climate::CLIMATE_MODE_COOL:
      second_frame[PANAAC_BYTEPOS_POWER] |= PANAAC_POWER_ON;
      second_frame[PANAAC_BYTEPOS_MODE] |= PANAAC_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      second_frame[PANAAC_BYTEPOS_POWER] |= PANAAC_POWER_ON;
      second_frame[PANAAC_BYTEPOS_MODE] |= PANAAC_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_DRY:
      second_frame[PANAAC_BYTEPOS_POWER] |= PANAAC_POWER_ON;
      second_frame[PANAAC_BYTEPOS_MODE] |= PANAAC_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      second_frame[PANAAC_BYTEPOS_POWER] |= PANAAC_POWER_ON;
      second_frame[PANAAC_BYTEPOS_MODE] |= PANAAC_MODE_FAN_ONLY;
      break;
    case climate::CLIMATE_MODE_AUTO:
      second_frame[PANAAC_BYTEPOS_POWER] |= PANAAC_POWER_ON;
      second_frame[PANAAC_BYTEPOS_MODE] |= PANAAC_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      second_frame[PANAAC_BYTEPOS_POWER] |= PANAAC_POWER_OFF;
      second_frame[PANAAC_BYTEPOS_MODE] |= PANAAC_MODE_COOL;
      break;
  }

  // temperature
  uint8_t encoded_temp = static_cast<uint8_t>(state.temp) - PANAAC_TEMP_MIN;
  encoded_temp &= 0x0F;
  second_frame[PANAAC_BYTEPOS_TEMP] = 0x20 | (encoded_temp << 1);
  if (static_cast<uint8_t>(state.temp) < state.temp)
    second_frame[PANAAC_BYTEPOS_TEMP] |= 0x01;

  // fan
  if (state.fan_level == PANAAC_FAN_QUIET && caps.supports_quiet) {
    second_frame[PANAAC_BYTEPOS_QUIET] |= PANAAC_FAN_QUIET;
  }
  second_frame[PANAAC_BYTEPOS_FAN] |= state.fan_level;

  // Panasonic preset bits. POWERFUL and ECO are mutually exclusive.
  if (state.preset == PANAAC_PRESET_POWERFUL && caps.supports_powerful)
    second_frame[PANAAC_BYTEPOS_POWERFUL] |= PANAAC_POWERFUL;
  else if (state.preset == PANAAC_PRESET_ECO && caps.supports_eco)
    second_frame[PANAAC_BYTEPOS_ECO] |= PANAAC_ECO;

  // nanoe-G air purifier flag. The remote's normal frames always carry bit 7 of byte 17; mirror
  // that when the feature is enabled (the legacy frame, without the feature, is unchanged).
  if (caps.supports_nanoe_g) {
    second_frame[PANAAC_BYTEPOS_NANOE_G] |= PANAAC_NANOE_FRAME_MARK;
    if (state.nanoe_g)
      second_frame[PANAAC_BYTEPOS_NANOE_G] |= PANAAC_NANOE_G;
  }

  // swing
  second_frame[PANAAC_BYTEPOS_SWINGV] |= state.swing_v_pos;
  if (caps.swing_horizontal) {
    second_frame[PANAAC_BYTEPOS_SWINGH] |= state.swing_h_pos;
  } else {
    second_frame[PANAAC_BYTEPOS_SWINGH] |= PANAAC_SWINGH_NONE;
  }

  // timers + clock (the remote's timer layout)
  if (timer.include) {
    if (state.on_timer_enabled)
      second_frame[PANAAC_BYTEPOS_POWER] |= PANAAC_TIMER_ON_FLAG;
    if (state.off_timer_enabled)
      second_frame[PANAAC_BYTEPOS_POWER] |= PANAAC_TIMER_OFF_FLAG;
    // Disabled timers are sent as "none" (0x600); bit 11 of each field is always set.
    const uint16_t on_field =
        PANAAC_TIMER_FIELD_FLAG | (state.on_timer_enabled ? (state.on_timer_minutes % PANAAC_TIMER_MAX_MINUTES)
                                                          : PANAAC_TIMER_NONE);
    const uint16_t off_field =
        PANAAC_TIMER_FIELD_FLAG | (state.off_timer_enabled ? (state.off_timer_minutes % PANAAC_TIMER_MAX_MINUTES)
                                                           : PANAAC_TIMER_NONE);
    second_frame[PANAAC_BYTEPOS_TIMER_ON] = static_cast<uint8_t>(on_field & 0xFF);
    second_frame[PANAAC_BYTEPOS_TIMER_MIX] =
        static_cast<uint8_t>(((on_field >> 8) & 0x0F) | ((off_field & 0x0F) << 4));
    second_frame[PANAAC_BYTEPOS_TIMER_OFF] = static_cast<uint8_t>((off_field >> 4) & 0xFF);
    second_frame[PANAAC_BYTEPOS_TIMER_MARK] = PANAAC_TIMER_FRAME_MARK;
    const uint16_t clock = timer.clock_minutes % PANAAC_TIMER_MAX_MINUTES;
    second_frame[PANAAC_BYTEPOS_CLOCK_LO] = static_cast<uint8_t>(clock & 0xFF);
    // Byte 17: clock high bits in bits 0-2; keep only the ECO bit (bit 4) of what was set above.
    second_frame[PANAAC_BYTEPOS_CLOCK_HI] =
        static_cast<uint8_t>((second_frame[PANAAC_BYTEPOS_ECO] & PANAAC_ECO) | ((clock >> 8) & 0x07));
  }

  // checksum
  for (uint8_t i = 0; i < 18; i++)
    second_frame[18] += second_frame[i];

  return second_frame;
}

/// What a received 19-byte frame says about the timers.
struct TimerFields {
  bool is_timer_frame{false};
  bool on_enabled{false};
  bool off_enabled{false};
  uint16_t on_minutes{0};
  uint16_t off_minutes{0};
};

/// Timer/clock frames are recognised by byte 15 = 0x80 (normal frames carry 0x89).
inline TimerFields decode_timer_fields(const uint8_t *f) {
  TimerFields t;
  if (f[PANAAC_BYTEPOS_TIMER_MARK] != PANAAC_TIMER_FRAME_MARK)
    return t;
  t.is_timer_frame = true;
  const uint16_t on_field = static_cast<uint16_t>(f[PANAAC_BYTEPOS_TIMER_ON] | ((f[PANAAC_BYTEPOS_TIMER_MIX] & 0x0F) << 8));
  const uint16_t off_field =
      static_cast<uint16_t>((f[PANAAC_BYTEPOS_TIMER_MIX] >> 4) | (f[PANAAC_BYTEPOS_TIMER_OFF] << 4));
  const uint16_t on_val = on_field & 0x7FF;
  const uint16_t off_val = off_field & 0x7FF;
  t.on_enabled = (f[PANAAC_BYTEPOS_POWER] & PANAAC_TIMER_ON_FLAG) != 0 && on_val < PANAAC_TIMER_MAX_MINUTES;
  t.off_enabled = (f[PANAAC_BYTEPOS_POWER] & PANAAC_TIMER_OFF_FLAG) != 0 && off_val < PANAAC_TIMER_MAX_MINUTES;
  t.on_minutes = t.on_enabled ? on_val : 0;
  t.off_minutes = t.off_enabled ? off_val : 0;
  return t;
}

/// Parse "H:MM" / "HH:MM" into minutes since 00:00. Returns false on anything else.
inline bool parse_hhmm(const char *s, uint16_t &minutes) {
  if (s == nullptr)
    return false;
  unsigned h = 0, m = 0;
  int digits = 0;
  while (*s >= '0' && *s <= '9' && digits < 2) {
    h = h * 10 + static_cast<unsigned>(*s - '0');
    s++;
    digits++;
  }
  if (digits == 0 || *s != ':')
    return false;
  s++;
  digits = 0;
  while (*s >= '0' && *s <= '9' && digits < 2) {
    m = m * 10 + static_cast<unsigned>(*s - '0');
    s++;
    digits++;
  }
  if (digits != 2 || *s != '\0' || h > 23 || m > 59)
    return false;
  minutes = static_cast<uint16_t>(h * 60 + m);
  return true;
}

}  // namespace esphome::panaac_v2
