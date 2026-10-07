# Copyright 2026 Minh Hoang
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import esphome.codegen as cg
from esphome.components import climate, remote_base, select, sensor
from esphome.components import time as time_
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.const import (
    CONF_DISABLED_BY_DEFAULT,
    CONF_DEVICE_ID,
    CONF_ICON,
    CONF_ID,
    CONF_INTERNAL,
    CONF_MQTT_ID,
    CONF_NAME,
    CONF_SENSOR,
    CONF_TIME_ID,
)
from esphome.components.remote_base import CONF_RECEIVER_ID, CONF_TRANSMITTER_ID
from esphome.types import ConfigType

AUTO_LOAD = ["climate", "select", "sensor"]
DEPENDENCIES = ["remote_transmitter", "climate"]
# `mqtt` is a soft dependency: the v2 MQTT features (topic_prefix) require a `mqtt:` block, which
# defines USE_MQTT and compiles in the CustomMQTTDevice code path. Without a `mqtt:` block
# (v1 native mode) USE_MQTT is undefined and all MQTT code is compiled out, so no broker is
# needed — the component behaves like PanaAC v1.

panaac_v2_ns = cg.esphome_ns.namespace('panaac_v2')
PanaACV2Climate = panaac_v2_ns.class_(
    'PanaACV2Climate',
    climate.Climate,
    cg.Component,
    remote_base.RemoteReceiverListener,
    remote_base.RemoteTransmittable,
)
PanaACV2SwingV = panaac_v2_ns.class_('PanaACV2SwingV', select.Select, cg.Component)
PanaACV2SwingH = panaac_v2_ns.class_('PanaACV2SwingH', select.Select, cg.Component)

CONF_TOPIC_PREFIX = "topic_prefix"
CONF_HIDE_LEGACY_COMPS = "hide_legacy_comps"
CONF_SUPPORTS_COOL = "supports_cool"
CONF_SUPPORTS_HEAT = "supports_heat"
CONF_SUPPORTS_FAN_ONLY = "supports_fan_only"
CONF_SUPPORTS_QUIET = "supports_quiet"
CONF_SUPPORTS_POWERFUL = "supports_powerful"
CONF_SUPPORTS_ECO = "supports_eco"
CONF_SUPPORTS_NANOE_G = "supports_nanoe_g"
CONF_SUPPORTS_SLEEP = "supports_sleep"
CONF_SUPPORTS_TIMER = "supports_timer"
CONF_FAN_5LEVEL = "fan_5level"
CONF_SWING_HORIZONTAL = "swing_horizontal"
CONF_TEMP_STEP = "temp_step"
CONF_IR_CONTROL = "ir_control"

CONF_SWINGV_ID = "swingv_id"
CONF_SWINGH_ID = "swingh_id"

def _validate_temp_step(value):
    """Accept only target-temperature steps representable by the IR protocol."""
    step = cv.float_(value)
    if step not in (0.5, 1.0):
        raise cv.Invalid("temp_step must be either 0.5 or 1.0")
    return step



CONFIG_SCHEMA = climate.climate_schema(PanaACV2Climate).extend({
    cv.Required(CONF_RECEIVER_ID): cv.use_id(remote_base.RemoteReceiverBase),
    cv.Required(CONF_TRANSMITTER_ID): cv.use_id(remote_base.RemoteTransmitterBase),
    cv.Optional(CONF_TOPIC_PREFIX): cv.string,
    cv.Optional(CONF_HIDE_LEGACY_COMPS, default=False): cv.boolean,
    cv.Optional(CONF_SUPPORTS_COOL, default=True): cv.boolean,
    cv.Optional(CONF_SUPPORTS_HEAT, default=False): cv.boolean,
    cv.Optional(CONF_SUPPORTS_FAN_ONLY, default=False): cv.boolean,
    cv.Optional(CONF_SUPPORTS_QUIET, default=False): cv.boolean,
    cv.Optional(CONF_SUPPORTS_POWERFUL, default=False): cv.boolean,
    cv.Optional(CONF_SUPPORTS_ECO, default=False): cv.boolean,
    cv.Optional(CONF_SUPPORTS_NANOE_G, default=False): cv.boolean,
    cv.Optional(CONF_SUPPORTS_SLEEP, default=False): cv.boolean,
    cv.Optional(CONF_SUPPORTS_TIMER, default=False): cv.boolean,
    cv.Optional(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
    cv.Optional(CONF_FAN_5LEVEL, default=False): cv.boolean,
    cv.Optional(CONF_SWING_HORIZONTAL, default=False): cv.boolean,
    cv.Optional(CONF_TEMP_STEP, default=1.0): _validate_temp_step,
    cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_IR_CONTROL, default=False): cv.boolean,
    cv.GenerateID(CONF_SWINGV_ID): cv.declare_id(PanaACV2SwingV),
    cv.GenerateID(CONF_SWINGH_ID): cv.declare_id(PanaACV2SwingH),
}).extend(cv.COMPONENT_SCHEMA).extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA).extend(remote_base.REMOTE_LISTENER_SCHEMA)


async def _make_select(select_id, name, icon, parent, device_id=None, hide=False):
    """Create one companion select with an icon and optional ESPHome sub-device assignment.
    Options are filled at runtime in PanaACV2Climate::setup(). When hide is true the select is
    made internal (hidden from the native API / Home Assistant)."""
    cfg = {CONF_ID: select_id, CONF_NAME: name, CONF_ICON: icon, CONF_DISABLED_BY_DEFAULT: False}
    if device_id is not None:
        cfg[CONF_DEVICE_ID] = device_id
    if hide:
        cfg[CONF_INTERNAL] = True
    sel = cg.new_Pvariable(select_id)
    await select.register_select(sel, cfg, options=[])
    await cg.register_component(sel, cfg)
    cg.add(sel.set_parent_climate(parent))
    return sel


async def to_code(config):
    mqtt_enabled = CONF_TOPIC_PREFIX in config

    # In v2 mode the climate is exposed via the custom PanaAC v2 MQTT topics; prevent the
    # climate platform from also creating a standard MQTT climate component. In v1 mode keep
    # it so the climate gets standard MQTT discovery (like PanaAC v1) when a `mqtt:` block is
    # present.
    if mqtt_enabled:
        config.pop(CONF_MQTT_ID, None)

    # hide_legacy_comps hides the on-device "(v1)" climate + its Swing V/H selects from the
    # native API (and so from Home Assistant) so they do not duplicate the full PanaAC v2 climate
    # card that the PanaAC v2 HA custom integration exposes over MQTT. It only takes effect in v2
    # mode (topic_prefix set): in v1 mode the climate + selects ARE the user-facing entities and
    # must stay visible, so the flag is forced off there regardless of its YAML value.
    hide_legacy = config[CONF_HIDE_LEGACY_COMPS] and mqtt_enabled
    device_id = config.get(CONF_DEVICE_ID)

    # Append the "(v1)" suffix to the climate name so the on-device climate is never mistaken for
    # the full PanaAC v2 climate card (which in v2 mode comes from the PanaAC v2 HA custom
    # integration over MQTT). Applied in BOTH modes, before new_climate() so the entity name and
    # object_id hash both reflect the suffixed name.
    climate_name = config.get(CONF_NAME) or ""
    if "(v1)" not in climate_name:
        config[CONF_NAME] = f"{climate_name} (v1)".strip()

    # Hiding is done by setting the standard `internal` entity flag before the climate/selects
    # are registered: internal entities are skipped by the api and mqtt components, so they never
    # reach Home Assistant, while the component's own custom-MQTT code (v2 traits/state/set) still
    # runs. The custom v2 MQTT publishing is independent of the native ClimateTraits/entity
    # registration, so hiding does not affect the HA integration's climate card.
    if hide_legacy:
        config[CONF_INTERNAL] = True

    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await remote_base.register_listener(var, config)
    await remote_base.register_transmittable(var, config)

    cg.add(var.set_mqtt_enabled(mqtt_enabled))
    if mqtt_enabled:
        cg.add(var.set_topic_prefix(config[CONF_TOPIC_PREFIX]))
        # The on-device "(v1)" climate stays VISIBLE on the native API alongside the
        # two Swing V/H selects, at the root of the ESPHome device — exactly like
        # PanaAC v1 — UNLESS hide_legacy_comps is true (v2 mode), in which case all three are made
        # internal above/below and hidden from Home Assistant. The full-featured PanaAC v2 climate
        # card is still provided by the PanaAC v2 HA custom integration over the custom MQTT topics
        # below. CONF_MQTT_ID is dropped above so ESPHome does not ALSO publish a standard MQTT
        # climate component that would duplicate the HA-integration climate; the native API is the
        # transport for the (optionally hidden) legacy climate.

    cg.add(var.set_supports_cool(config[CONF_SUPPORTS_COOL]))
    cg.add(var.set_supports_heat(config[CONF_SUPPORTS_HEAT]))
    cg.add(var.set_supports_fan_only(config[CONF_SUPPORTS_FAN_ONLY]))
    cg.add(var.set_supports_quiet(config[CONF_SUPPORTS_QUIET]))
    cg.add(var.set_supports_powerful(config[CONF_SUPPORTS_POWERFUL]))
    cg.add(var.set_supports_eco(config[CONF_SUPPORTS_ECO]))
    cg.add(var.set_supports_nanoe_g(config[CONF_SUPPORTS_NANOE_G]))
    cg.add(var.set_supports_sleep(config[CONF_SUPPORTS_SLEEP]))
    cg.add(var.set_supports_timer(config[CONF_SUPPORTS_TIMER]))
    if time_id := config.get(CONF_TIME_ID):
        cg.add(var.set_time(await cg.get_variable(time_id)))
    cg.add(var.set_fan_5level(config[CONF_FAN_5LEVEL]))
    cg.add(var.set_swing_horizontal(config[CONF_SWING_HORIZONTAL]))
    cg.add(var.set_temp_step(config[CONF_TEMP_STEP]))
    cg.add(var.set_ir_control(config[CONF_IR_CONTROL]))
    if sensor_id := config.get(CONF_SENSOR):
        sens = await cg.get_variable(sensor_id)
        cg.add(var.set_sensor(sens))

    # Companion Swing V/H selects (PanaAC v1 features), created in BOTH modes — the granular swing
    # positions are not on the climate card. When device_id is configured, these selects are
    # grouped with the climate under that ESPHome sub-device. Fan levels are NOT a select: they are the climate's
    # custom fan modes (Fan Mode) in both modes, so no Fan Level select is created.
    swingv = await _make_select(config[CONF_SWINGV_ID], "Swing Vertical",
                                "mdi:arrow-expand-vertical", var, device_id=device_id,
                                hide=hide_legacy)
    cg.add(var.set_swingv(swingv))
    if config[CONF_SWING_HORIZONTAL]:
        swingh = await _make_select(config[CONF_SWINGH_ID], "Swing Horizontal",
                                    "mdi:arrow-expand-horizontal", var, device_id=device_id,
                                    hide=hide_legacy)
        cg.add(var.set_swingh(swingh))


def _final_validate(config: ConfigType) -> ConfigType:
    """Build-time guard for the v2 MQTT mode switch (Codex review issue 3).

    `topic_prefix` enables v2 MQTT mode, which needs a global `mqtt:` block so `USE_MQTT` is
    defined and the `CustomMQTTDevice` code path is compiled in. Without a `mqtt:` block the
    firmware still builds, but `USE_MQTT` is undefined, all MQTT code is compiled out, and the
    HA-facing MQTT contract never comes up — the device would only log `v2 MQTT mode requires a
    mqtt: block` at runtime. Fail generation here instead, so the misconfiguration is caught
    before flashing.
    """
    if config.get(CONF_SUPPORTS_TIMER) and CONF_TIME_ID not in config:
        raise cv.Invalid(
            "`supports_timer: true` needs `time_id`: the Panasonic remote's timer frames carry the "
            "current clock, so add a `time:` component (e.g. platform: homeassistant or sntp) and "
            "reference it with `time_id`."
        )
    if CONF_TOPIC_PREFIX in config:
        full_config = fv.full_config.get()
        if not full_config.get("mqtt"):
            raise cv.Invalid(
                "The panaac_v2 `topic_prefix` option enables v2 MQTT mode, which requires a "
                "global `mqtt:` block in the configuration (it defines USE_MQTT and compiles in "
                "the MQTT code path). Add an `mqtt:` block with your broker settings, or remove "
                "`topic_prefix` to use v1 native mode (no broker needed)."
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate
