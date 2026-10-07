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
#include "extra.h"
#include "frame.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include <array>
#include <cstddef>
#include <span>
#include <string>

#ifdef USE_MQTT
#include "esphome/components/mqtt/custom_mqtt_device.h"
#endif
#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome::panaac_v2 {

/** PanaAC v2 — Panasonic AC controller with two exposure modes.
 *
 * The IR encode/decode core and the canonical `ac_state` (mode/temp/fan_level/swing positions)
 * are shared. What differs is how the state is exposed to Home Assistant:
 *
 * - **v1 native mode** (`topic_prefix` unset, `mqtt_enabled_ = false`): a normal native climate
 *   whose Fan Mode offers the full Panasonic fan levels (Auto / Level 1..5 / Quiet) as custom fan
 *   modes plus standard Auto/Quiet enums, visible on the ESPHome API / standard MQTT discovery,
 *   plus two companion `select` entities (Swing Vertical / Swing Horizontal) for the granular swing
 *   positions. Behaves like PanaAC v1. No MQTT broker is required — all MQTT code is compiled out
 *   (`USE_MQTT` undefined). The climate + selects sit at the root of the ESPHome device, like
 *   PanaAC v1.
 * - **v2 MQTT mode** (`topic_prefix` set, `mqtt_enabled_ = true`, requires a `mqtt:` block so
 *   `USE_MQTT` is defined): the climate is exposed over the custom
 *   `<prefix>/state|traits|availability|set` MQTT JSON topics consumed by the PanaAC v2 HA custom
 *   integration (the single all-in-one v2 climate card). It is ALSO kept visible on the native
 *   API as a "(v1)" climate — standard swing modes + custom fan-level strings + the same
 *   two Swing V/H selects — at the root of the ESPHome device, exactly like PanaAC
 *   v1. The custom MQTT topics are independent of the native ClimateTraits (publish_traits_() is
 *   hand-rolled JSON), so the v2 HA-integration card is unaffected by the visible native climate.
 *
 * `ac_state` is the single source of truth; the Climate base fields and (in v2 mode) the custom
 * fan/swing strings are derived from it via `sync_to_climate_()`.
 */
class PanaACV2Climate : public climate::Climate,
                        public Component,
#ifdef USE_MQTT
                        public mqtt::CustomMQTTDevice,
#endif
                        public remote_base::RemoteReceiverListener,
                        public remote_base::RemoteTransmittable {
 public:
  PanaACV2Climate();

  void set_topic_prefix(const std::string &topic_prefix) { this->topic_prefix_ = topic_prefix; }
  void set_mqtt_enabled(bool enabled) { this->mqtt_enabled_ = enabled; }
  void set_supports_cool(bool supports) { this->supports_cool_ = supports; }
  void set_supports_heat(bool supports) { this->supports_heat_ = supports; }
  void set_supports_fan_only(bool supports) { this->supports_fan_only_ = supports; }
  void set_supports_quiet(bool supports) { this->supports_quiet_ = supports; }
  void set_supports_powerful(bool supports) { this->supports_powerful_ = supports; }
  void set_supports_eco(bool supports) { this->supports_eco_ = supports; }
  void set_supports_nanoe_g(bool supports) { this->supports_nanoe_g_ = supports; }
  void set_supports_sleep(bool supports) { this->supports_sleep_ = supports; }
  void set_supports_timer(bool supports) { this->supports_timer_ = supports; }
#ifdef USE_TIME
  void set_time(time::RealTimeClock *time) { this->time_ = time; }
#endif
  void set_fan_5level(bool fan_5level) { this->fan_5level_ = fan_5level; }
  void set_swing_horizontal(bool swing_horizontal) { this->swing_horizontal_ = swing_horizontal; }
  void set_temp_step(float temp_step) { this->temp_step_ = temp_step; }
  void set_ir_control(bool ir_control) { this->ir_control_ = ir_control; }
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

  void set_swingv(PanaACV2SwingV *swingv) { this->swingv_ = swingv; }
  void set_swingh(PanaACV2SwingH *swingh) { this->swingh_ = swingh; }

  /// Entry points used by the companion Swing V/H selects (both modes). Each validates, mutates
  /// `ac_state`, transmits the IR frame, publishes (by mode), and re-syncs the other select.
  void apply_swingv_select_(SwingVPos pos);
  void apply_swingh_select_(SwingHPos pos);

  /// Extra remote functions. Each is a no-op (with a warning) unless enabled in the YAML
  /// (`supports_nanoe_g` / `supports_sleep` / `supports_timer`). Usable from lambdas and template entities.
  /// nanoe-G is a state flag carried in every transmitted frame: this updates ac_state, publishes
  /// and transmits (it always transmits, so it also re-syncs an AC that drifted out of sync).
  void set_nanoe_g(bool on);
  /// SLEEP is a stepping command like the remote's button: each call sends the next of the 11 captured
  /// steps (wrapping after the last). The AC does not report it back.
  void send_sleep();
  /// Send a specific SLEEP step (0..10, in the order the remote cycles through them).
  void send_sleep_step(uint8_t step);
  /// ON / OFF timers (minutes since 00:00, 0..1439). They need `time_id` (the remote's frames carry
  /// the clock). While a timer is enabled every transmitted frame carries the timers and the clock;
  /// nanoe-G is not carried in those frames.
  void set_on_timer(uint16_t minutes);
  void set_off_timer(uint16_t minutes);
  /// Cancel both timers (sends the timer layout with the enable bits cleared, like the remote's CANCEL).
  void cancel_timers();

  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  /// Public by design: the companion select entities read/write it directly (like v1).
  ClimateState ac_state;

 protected:
  /// Climate integration interface.
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  bool on_receive(remote_base::RemoteReceiveData data) override;

  // IR core (operate on ac_state).
  void transmit_data_(bool force_timer_frame = false);
  /// Timer/clock info for the next state frame (include = false when timers are off or the clock is unknown).
  TimerFrameInfo timer_frame_info_(bool force);
  /// Transmit the fixed first frame followed by `second_frame` (19-byte state frame or the 8-byte
  /// SLEEP frame). Blocks for the duration of the signal (~260 ms).
  void transmit_frames_(std::span<const uint8_t> second_frame);
  FrameCaps frame_caps_() const;
  bool decode_data_(remote_base::RemoteReceiveData data, std::array<uint8_t, 27> &state_bytes, size_t &state_len);
  bool decode_state_(std::span<const uint8_t> state_bytes, ClimateState &state);
  bool decode_and_apply_(std::span<const uint8_t> state_bytes);

  // State synchronization helpers.
  void sync_to_climate_();         // derive Climate base fields (+ v2 custom strings) from ac_state
  void update_selects_();          // push ac_state fan/swing positions to the select entities
  void publish_state_by_mode_();   // v2: MQTT publish_state_() + publish_state(); v1: publish_state()
  void recompute_swing_mode_();    // derive ac_state.swing_mode from swing_v_pos / swing_h_pos
  void update_action_();           // derive Climate action from the commanded mode (one-way IR)

#ifdef USE_MQTT
  // v2 MQTT publish helpers (only used when mqtt_enabled_).
  void publish_state_();
  void publish_traits_();
  void on_set_json_(const std::string &topic, JsonObject root);
  bool set_swing_mode_if_supported_(const char *mode);
  bool set_swing_horizontal_mode_if_supported_(const char *mode);

  std::string state_topic_() const { return this->topic_prefix_ + "/state"; }
  std::string traits_topic_() const { return this->topic_prefix_ + "/traits"; }
  std::string set_topic_() const { return this->topic_prefix_ + "/set"; }
  std::string availability_topic_() const { return this->topic_prefix_ + "/availability"; }
#endif

  std::string topic_prefix_;
  bool mqtt_enabled_{false};

  bool supports_cool_{true};
  bool supports_heat_{false};
  bool supports_powerful_{false};
  bool supports_eco_{false};
  bool supports_nanoe_g_{false};
  bool supports_sleep_{false};
  bool supports_timer_{false};
  size_t sleep_step_{PANAAC_SLEEP_STEPS.size() - 1};  // last step sent; the first press sends step 0
#ifdef USE_TIME
  time::RealTimeClock *time_{nullptr};
#endif
  bool timer_cancel_pending_{false};
  bool supports_fan_only_{false};
  bool supports_quiet_{false};
  bool fan_5level_{false};
  bool swing_horizontal_{false};
  float temp_step_{1.0f};
  bool ir_control_{false};
  sensor::Sensor *sensor_{nullptr};

  PanaACV2SwingV *swingv_{nullptr};
  PanaACV2SwingH *swingh_{nullptr};

  // v2 custom swing strings (derived from ac_state; published over MQTT). Kept as members so
  // publish_state_() and the v2 set path can read them without re-deriving each time.
  const char *custom_swing_mode_{STR_SWINGV_MIDDLE};
  const char *swing_horizontal_mode_{STR_SWINGH_MIDDLE};

  bool traits_published_{false};

  // v2 MQTT set-command atomicity (Codex review issue 2). While mqtt_command_active_ is set,
  // control() (reached via call.perform() from on_set_json_()) updates ac_state and records a
  // pending change in pending_change_ but does NOT publish or transmit, so a single MQTT
  // command mixing standard climate fields with Panasonic swing fields emits at most one IR
  // burst. on_set_json_() performs the single publish/transmit once all fields are applied.
  bool mqtt_command_active_{false};
  bool pending_change_{false};
};

}  // namespace esphome::panaac_v2
