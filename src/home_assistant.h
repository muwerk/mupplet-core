// home_assistant.h - muwerk HomeAssistant Autodiscovery Manager
#pragma once

#include "muwerk.h"
#include "mupplet_core.h"
#include "jsonfile.h"

#define USTD_FEATURE_HOMEASSISTANT

namespace ustd {
/** mupplet-core HomeAssistant Device Autodiscovery Helper

This class implements support for the "MQTT Discovery" feature of HomeAssistant. By creating
an instance of this class, and by specifiying the affected entites, the device will be able
to advertise the exported entites to HomeAsisstant using the specified MWTT messages.
(See https://www.home-assistant.io/docs/mqtt/discovery/ for details)

## Messages

### Incoming Messages

The HomeAssistant helper reacts to the following messages:

| Topic            | Message Body  | Description
| ---------------- | ------------- | -------------------------------------------------
| `ha/state/set`   | `on` or `off` | Enables or disables entity discovery.
| `ha/state/get`   |               | Requests the current entity discovery state.

### Outgoing Messages

| Topic                        | Message Body  | Description
| ---------------------------- | ------------- | ---------------------------------------
| `ha/state`                   | `on` or `off` | Current State of the entity discovery
| `ha/attribs/<attribGroup>`   | `{ ... }`     | Current entity attributes (See below)

Entity attributes are sent as JSON object and are displayed as attributes to an entity.
The HomeAssistant Device Autodiscovery Helper always sends the attribute group `device`
that will contain such data:

\code{json}
{
  "RSSI": "52",
  "Signal (dBm)": "-74",
  "Mac": "84:F3:EB:1A:2F:D8",
  "IP": "192.168.107.241",
  "Host": "test-esp-d1mini-01",
  "Manufacturer": "Starfleet Engineering",
  "Model": "Tricorder Mark VII",
  "Version": "3.14.15"
}
\endcode

## Sample Integration

\code{cpp}
#define __ESP__ 1   // Platform defines required, see ustd library doc, mainpage.
#include "scheduler.h"
#include "net.h"
#include "mqtt.h"
#include "home_assistant.h"
#include "mup_airq_bme280.h"
#include "mup_light_pca9685.h"

ustd::Scheduler sched;
ustd::AirQualityBme280 bme( "bme280", BME280_ADDRESS_ALTERNATE,
                            ustd::AirQualityBme280::SampleMode::MEDIUM );
ustd::LightsPCA9685 panel("panel");
ustd::HomeAssistant ha("Tricorder", "Starfleet Engineering", "Tricorder Mark VII", "3.14.15");

void setup() {
    net.begin(&sched);
    ota.begin(&sched);
    mqtt.begin(&sched);
    bme.begin(&sched);
    panel.begin(&sched);
    ha.begin(&sched, true);

    ha.addSensor("bme280","temperature");
    ha.addSensor("bme280","humidity");
    ha.addSensor("bme280","pressure");
    ha.addMultiLight("panel", 16);
}
\endcode

*/
class HomeAssistant {
  public:
    static const char *version;  // = "0.1.0";

    /// \brief HomeAssistant Device Type
    enum DeviceType {
        Unknown = -1,
        Sensor = 0,    /// Sensor reporting numerical or state values
        BinarySensor,  /// Sensor reporting binary states
        Switch,        /// A simple device that can only be switched on and off
        Light,         /// A simple light that can only be switched on and off
        LightDim,      /// A light that can be dimmed
        LightWW,       /// A light with light temperature that can be dimmed
        LightRGB,      /// A color light
        LightRGBW,     /// A color light with additional white component
        LightRGBWW     /// A color light with additional white compoenent that can be "tempered"
    };

  private:
    // internal type definitions
    typedef struct {
        const char *name;
        const char *manufacturer;
        const char *model;
        const char *version;
    } Attributes;

    typedef struct {
        DeviceType type;
        const char *name;
        const char *value;
        const char *human;
        const char *unit;
        const char *icon;
        const char *val_tpl;
        const char *attribs;
        const char *effects;
        const char *dev_cla;
        int channel;
        int off_dly;
        int exp_aft;
        bool frc_upd;
    } Entity;

    // muwerk task management
    Scheduler *pSched;
    int tID = SCHEDULER_MAIN;

#ifdef USTD_FEATURE_FILESYSTEM
    // configuration management
    jsonfile config;
#endif

    // device data
    String deviceName;
    String deviceManufacturer;
    String deviceModel;
    String deviceVersion;
    String deviceId;
    String haTopicAttrib = "ha/attribs/";
    String haTopicConfig = "!!homeassistant/";

    // runtime - states
    bool autodiscovery = false;
    bool connected = false;
    long rssiVal = -99;
    String macAddress;
    String ipAddress;
    String hostName;
    String pathPrefix;
    String lastWillTopic;
    String lastWillMessage;

    // runtime - device data
    ustd::array<Attributes> attribGroups;
    ustd::array<Entity> entityConfigs;

  public:
    /** Instantiate a HomeAssistant Autodiscovery Helper
     *
     * No interaction is performed, until \ref begin() is called.
     *
     * @param name Name of the device
     * @param manufacturer Manufacutrer of the deivce
     * @param model Model of the device
     * @param version Version of the device
     */
    HomeAssistant(String name, String manufacturer, String model, String version)
        : deviceName(name), deviceManufacturer(manufacturer), deviceModel(model),
          deviceVersion(version) {
    }

    virtual ~HomeAssistant() {
        for (unsigned int i = 0; i < attribGroups.length(); i++) {
            free((void *)attribGroups[i].name);
        }
        for (unsigned int i = 0; i < entityConfigs.length(); i++) {
            free((void *)entityConfigs[i].name);
        }
    }

    /** Initialize the HomeAssistant discovery helper and start operation
     * @param _pSched Pointer to a muwerk scheduler object, used to create worker
     *                tasks and for message pub/sub.
     * @param initialAutodiscovery Initial state of the HomeAssistant Autodiscovery Helper if
     * not already saved into the filesystem.
     */
    void begin(Scheduler *_pSched, bool initialAutodiscovery = false) {
        pSched = _pSched;

        // initialize configuration
#ifdef USTD_FEATURE_FILESYSTEM
        autodiscovery = config.readBool("ha/autodiscovery", initialAutodiscovery);
        if ((deviceId = config.readString("net/deviceid")) == "") {
            // initialize device id to mac address
            deviceId = WiFi.macAddress();
            deviceId.replace(":", "");
        }
#else
        autodiscovery = initialAutodiscovery;
        deviceId = WiFi.macAddress();
        deviceId.replace(":", "");
#endif

        // try to initialze some stuff
        addAttributes("device");

        // react to network state changes
        pSched->subscribe(tID, "mqtt/config", [this](String topic, String msg, String orig) {
            this->onMqttConfig(topic, msg, orig);
        });
        pSched->subscribe(tID, "mqtt/state", [this](String topic, String msg, String orig) {
            this->onMqttState(topic, msg, orig);
        });
        pSched->subscribe(tID, "net/network", [this](String topic, String msg, String orig) {
            this->onNetNetwork(topic, msg, orig);
        });
        pSched->subscribe(tID, "net/rssi", [this](String topic, String msg, String orig) {
            this->onNetRssi(topic, msg, orig);
        });

        // react to commands
        pSched->subscribe(tID, "ha/state/#", [this](String topic, String msg, String originator) {
            this->onCommand(topic.substring(11), msg);
        });

        // request current state
        pSched->publish("net/network/get");
        pSched->publish("mqtt/state/get");

        // publish out current state
        publishState();
    }

    /** Activates or deactivates HomeAssistant MQTT discovery
     * @param enabled `true` if devices shall be advertised to HomeAssistant, `false` if not
     */
    void setAutoDiscovery(bool enabled) {
        if (autodiscovery != enabled) {
            autodiscovery = enabled;
            updateHA();
        }
        publishState();
#ifdef USTD_FEATURE_FILESYSTEM
        config.writeBool("ha/autodiscovery", autodiscovery);
#endif
    }

    /** Adds a specific attribute group for the device
     *
     * By adding an attribute group, the device sends a full set of attributes every time the
     * network state changes under the topic `ha/attribs/<attribGroup>`. The default attribute
     * group 'device' is already defined automatically using the device information supplied in
     * the constructor. Adding additional attribute groups is only useful if specific entities
     * should provide more detailed information about the manufacturer of the hardware and/or
     * the hardware/software revision.
     *
     * @param attribGroup Name of the attribute group
     * @param manufacturer (default is the value defined in the constructor). Name of the
     * manufacturer
     * @param model (default is the value defined in the constructor). Name of the model
     * @param version (default is the value defined in the constructor). Software or hardware
     * version information
     *
     */
    void addAttributes(String attribGroup, String manufacturer = "", String model = "",
                       String version = "") {

        for (unsigned int i = 0; i < attribGroups.length(); i++) {
            if (attribGroup == attribGroups[i].name) {
                // insert only unique attribute groups
                return;
            }
        }

        Attributes att = {};
        String *man = manufacturer.length() ? &manufacturer : &deviceManufacturer;
        String *mod = model.length() ? &model : &deviceModel;
        String *ver = version.length() ? &version : &deviceVersion;

        att.name = (char *)malloc(attribGroup.length() + man->length() + mod->length() +
                                  ver->length() + 4);
        if (att.name) {
            att.manufacturer = copyfieldgetnext(att.name, attribGroup);
            att.model = copyfieldgetnext(att.manufacturer, *man);
            att.version = copyfieldgetnext(att.model, *mod);
            strcpy((char *)att.version, ver->c_str());
            if (attribGroups.add(att) == -1) {
                free((void *)att.name);
            }
        }
    }

    /** Adds an entity definition for a switchable entity
     *
     * This method adds a definition for a switchable entity.
     * HomeAssistant will treat such as device as a "Switch". Switches are devices that can
     * be switched on or off like \ref DigitalOut.
     *
     * @param name Unique name of the referenced mupplet
     * @param human Optional human readable name for HomeAssistant entity
     * @param dev_cla Optional device class for the switch (See:
     * https://www.home-assistant.io/integrations/switch/)
     * @param icon Optional alternative icon
     * @param attribs Optional alternative attribute group (by default all entities reference the
     * "device" attributes group)
     */
    void addSwitch(String name, String human = "", String dev_cla = "", String icon = "",
                   String attribs = "") {
        addGenericActor(Switch, name, -1, human, dev_cla, icon, attribs);
    }

    /** Adds an entity definition for a switchable entity
     *
     * This method adds a definition for a specific channel of a multichannel switchable entity.
     * HomeAssistant will treat such as device as a "Switch". Switches are devices that can
     * be switched on or off like \ref DigitalOut.
     *
     * @param name Unique name of the referenced mupplet
     * @param channel Number of the channel
     * @param human Optional human readable name for HomeAssistant entity
     * @param dev_cla Optional device class for the switch (See:
     * https://www.home-assistant.io/integrations/switch/)
     * @param icon Optional alternative icon
     * @param attribs Optional alternative attribute group (by default all entities reference the
     * "device" attributes group)
     */
    void addSwitch(String name, int channel, String human = "", String dev_cla = "",
                   String icon = "", String attribs = "") {
        if (channel >= 0) {
            addGenericActor(Switch, name, channel, human, dev_cla, icon, attribs);
        }
    }

    /** Adds entity definitions for multiple switchable entities
     *
     * This method adds definitions for all channels of a multichannel switchable entity.
     * HomeAssistant will treat such as device as a "Switch". Switches are devices that can
     * be switched on or off like \ref DigitalOut.
     *
     * @param name Unique name of the referenced mupplet
     * @param count Number of channels of the referenced mupplet
     * @param human Optional human readable name for HomeAssistant entity
     * @param dev_cla Optional device class for the switch (See:
     * https://www.home-assistant.io/integrations/switch/)
     * @param icon Optional alternative icon
     * @param attribs Optional alternative attribute group (by default all entities reference the
     * "device" attributes group)
     */
    void addMultiSwitch(String name, int count, String human = "", String dev_cla = "",
                        String icon = "", String attribs = "") {
        if (count > 1) {
            addGenericActor(Switch, name, -count, human, dev_cla, icon, attribs);
        }
    }

    /** Adds an entity definition for a light entity
     *
     * This method adds a definition for light entity.
     * HomeAssistant will treat such a device as a "Light". Lights are devices that support
     * multiple operating modes like \ref Light or \ref LightsPCA9685
     * (See: https://www.home-assistant.io/integrations/light/)
     *
     * @param name Unique name of the referenced mupplet
     * @param human Optional human readable name for HomeAssistant entity
     * @param type Optional type of light (default: \ref LightDim)
     * @param icon Optional alternative icon
     * @param attribs Optional alternative attribute group (by default all entities reference the
     * "device" attributes group)
     * @param effects Optional string containing a comma-separated list of special effect names
     */
    void addLight(String name, String human = "", DeviceType type = LightDim, String icon = "",
                  String attribs = "", String effects = "") {
        if (type >= Light && type <= LightRGBWW) {
            addGenericActor(type, name, -1, human, "", icon, attribs, effects);
        }
    }

    /** Adds an entity definition for a light entity
     *
     * This method adds a definition for a specific channel of a multichannel light entity.
     * HomeAssistant will treat such a device as a "Light". Lights are devices that support
     * multiple operating modes like \ref Light or \ref LightsPCA9685
     * (See: https://www.home-assistant.io/integrations/light/)
     *
     * @param name Unique name of the referenced mupplet
     * @param channel Number of the channel
     * @param human Optional human readable name for HomeAssistant entity
     * @param type Optional type of light (default: \ref LightDim)
     * @param icon Optional alternative icon
     * @param attribs Optional alternative attribute group (by default all entities reference the
     * "device" attributes group)
     * @param effects Optional string containing a comma-separated list of special effect names
     */
    void addLight(String name, int channel, String human = "", DeviceType type = LightDim,
                  String icon = "", String attribs = "", String effects = "") {
        if (channel >= 0 && type >= Light && type <= LightRGBWW) {
            addGenericActor(type, name, channel, human, "", icon, attribs, effects);
        }
    }

    /** Adds entity definitions for multiple light entities
     *
     * This method adds a definition for all channels of a multichannel light entity.
     * HomeAssistant will treat such a device as a "Light". Lights are devices that support
     * multiple operating modes like \ref Light or \ref LightsPCA9685
     * (See: https://www.home-assistant.io/integrations/light/)
     *
     * @param name Unique name of the referenced mupplet
     * @param count Number of channels of the referenced mupplet
     * @param human Optional human readable name for HomeAssistant entity
     * @param type Optional type of light (default: \ref LightDim)
     * @param icon Optional alternative icon
     * @param attribs Optional alternative attribute group (by default all entities reference the
     * "device" attributes group)
     */
    void addMultiLight(String name, int count, String human = "", DeviceType type = LightDim,
                       String icon = "", String attribs = "") {
        if (count > 1 && type >= Light && type <= LightRGBWW) {
            addGenericActor(type, name, -count, human, "", icon, attribs);
        }
    }

    /** Adds an entity definition for a sensor
     *
     * This method adds a definition for a sensor entity.
     * HomeAssistant will treat such as device as a "Sensor". Sensors are devices that reports
     * any kind of values like thermometers or similar.
     *
     * @param name Unique name of the referenced mupplet
     * @param value Name of the value reported by the mupplet
     * @param human Optional human readable name for HomeAssistant entity
     * @param dev_cla Optional device class for the sensor (See:
     * https://www.home-assistant.io/integrations/sensor/)
     * @param unit Optional alternative unit for the reported data
     * @param icon Optional alternative icon
     * @param val_tpl Optional value template to extract the value
     * @param exp_aft Optional expiration time in seconds after which the value expires
     * @param frc_upd If `true` the value update will be notified also if the value was not changed
     * @param attribs Optional alternative attribute group (by default all entities reference the
     * "device" attributes group)
     */
    void addSensor(String name, String value, String human = "", String dev_cla = "",
                   String unit = "", String icon = "", String val_tpl = "", int exp_aft = -1,
                   bool frc_upd = false, String attribs = "") {
        addGenericSensor(Sensor, name, value, -1, human, dev_cla, unit, icon, val_tpl, exp_aft,
                         frc_upd, -1, attribs);
    }

    /** Adds an entity definition for asensor
     *
     * This method adds a definition for a specific channel of a multichannel sensor entity.
     * HomeAssistant will treat such as device as a "Sensor". Sensors are devices that reports
     * any kind of values like thermometers or similar.
     *
     * @param name Unique name of the referenced mupplet
     * @param value Name of the value reported by the mupplet
     * @param channel Number of the channel
     * @param human Optional human readable name for HomeAssistant entity
     * @param dev_cla Optional device class for the sensor (See:
     * https://www.home-assistant.io/integrations/sensor/)
     * @param unit Optional alternative unit for the reported data
     * @param icon Optional alternative icon
     * @param val_tpl Optional value template to extract the value
     * @param exp_aft Optional expiration time in seconds after which the value expires
     * @param frc_upd If `true` the value update will be notified also if the value was not changed
     * @param attribs Optional alternative attribute group (by default all entities reference the
     * "device" attributes group)
     */
    void addSensor(String name, String value, int channel, String human = "", String dev_cla = "",
                   String unit = "", String icon = "", String val_tpl = "", int exp_aft = -1,
                   bool frc_upd = false, String attribs = "") {
        if (channel >= 0) {
            addGenericSensor(Sensor, name, value, channel, human, dev_cla, unit, icon, val_tpl,
                             exp_aft, frc_upd, -1, attribs);
        }
    }

    /** Adds entity definitions for a multiple sensors
     *
     * This method adds a definition for all channels of a multichannel sensor entity.
     * HomeAssistant will treat such as device as a "Sensor". Sensors are devices that reports
     * any kind of values like thermometers or similar.
     *
     * @param name Unique name of the referenced mupplet
     * @param value Name of the value reported by the mupplet
     * @param count Number of channels of the referenced mupplet
     * @param human Optional human readable name for HomeAssistant entity
     * @param dev_cla Optional device class for the sensor (See:
     * https://www.home-assistant.io/integrations/sensor/)
     * @param unit Optional alternative unit for the reported data
     * @param icon Optional alternative icon
     * @param val_tpl Optional value template to extract the value
     * @param exp_aft Optional expiration time in seconds after which the value expires
     * @param frc_upd If `true` the value update will be notified also if the value was not changed
     * @param attribs Optional alternative attribute group (by default all entities reference the
     * "device" attributes group)
     */
    void addMultiSensor(String name, String value, int count, String human = "",
                        String dev_cla = "", String unit = "", String icon = "",
                        String val_tpl = "", int exp_aft = -1, bool frc_upd = false,
                        String attribs = "") {
        if (count > 1) {
            addGenericSensor(Sensor, name, value, -count, human, dev_cla, unit, icon, val_tpl,
                             exp_aft, frc_upd, -1, attribs);
        }
    }

    /** Adds an entity definition for a binary sensor
     *
     * This method adds a definition for a binary sensor entity.
     * HomeAssistant will treat such as device as a "Binary Sensor". Binary Sensors are devices that
     * reports binary values (on or off) like buttons (e.g. \ref Switch), remote controls, etc.
     *
     * @param name Unique name of the referenced mupplet
     * @param value Name of the value reported by the mupplet
     * @param human Optional human readable name for HomeAssistant entity
     * @param dev_cla Optional device class for the sensor (See:
     * https://www.home-assistant.io/integrations/binary_sensor/)
     * @param unit Optional alternative unit for the reported data
     * @param icon Optional alternative icon
     * @param val_tpl Optional value template to extract the value
     * @param exp_aft Optional expiration time in seconds after which the value expires
     * @param frc_upd If `true` the value update will be notified also if the value was not changed
     * @param off_dly Optional delay in seconds after which the sensor’s state will be updated back
     * to off
     * @param attribs Optional alternative attribute group (by default all entities reference the
     * "device" attributes group)
     */
    void addBinarySensor(String name, String value, String human = "", String dev_cla = "",
                         String unit = "", String icon = "", String val_tpl = "", int exp_aft = -1,
                         bool frc_upd = false, int off_dly = -1, String attribs = "") {
        addGenericSensor(BinarySensor, name, value, -1, human, dev_cla, unit, icon, val_tpl,
                         exp_aft, frc_upd, off_dly, attribs);
    }

    /** Adds an entity definition for a binary sensor
     *
     * This method adds a definition for a specific channel of a multichannel binary sensor entity.
     * HomeAssistant will treat such as device as a "Binary Sensor". Binary Sensors are devices that
     * reports binary values (on or off) like buttons (e.g. \ref Switch), remote controls, etc.
     *
     * @param name Unique name of the referenced mupplet
     * @param value Name of the value reported by the mupplet
     * @param channel Number of the channel
     * @param human Optional human readable name for HomeAssistant entity
     * @param dev_cla Optional device class for the sensor (See:
     * https://www.home-assistant.io/integrations/binary_sensor/)
     * @param unit Optional alternative unit for the reported data
     * @param icon Optional alternative icon
     * @param val_tpl Optional value template to extract the value
     * @param exp_aft Optional expiration time in seconds after which the value expires
     * @param frc_upd If `true` the value update will be notified also if the value was not changed
     * @param off_dly Optional delay in seconds after which the sensor’s state will be updated back
     * to off
     * @param attribs Optional alternative attribute group (by default all entities reference the
     * "device" attributes group)
     */
    void addBinarySensor(String name, String value, int channel, String human = "",
                         String dev_cla = "", String unit = "", String icon = "",
                         String val_tpl = "", int exp_aft = -1, bool frc_upd = false,
                         int off_dly = -1, String attribs = "") {
        if (channel >= 0) {
            addGenericSensor(BinarySensor, name, value, channel, human, dev_cla, unit, icon,
                             val_tpl, exp_aft, frc_upd, off_dly, attribs);
        }
    }

    /** Adds entity definitions for multiple binary sensors
     *
     * This method adds a definition for all channels of a multichannel binary sensor entity.
     * HomeAssistant will treat such as device as a "Binary Sensor". Binary Sensors are devices that
     * reports binary values (on or off) like buttons (e.g. \ref Switch), remote controls, etc.
     *
     * @param name Unique name of the referenced mupplet
     * @param value Name of the value reported by the mupplet
     * @param count Number of channels of the referenced mupplet
     * @param human Optional human readable name for HomeAssistant entity
     * @param dev_cla Optional device class for the sensor (See:
     * https://www.home-assistant.io/integrations/binary_sensor/)
     * @param unit Optional alternative unit for the reported data
     * @param icon Optional alternative icon
     * @param val_tpl Optional value template to extract the value
     * @param exp_aft Optional expiration time in seconds after which the value expires
     * @param frc_upd If `true` the value update will be notified also if the value was not changed
     * @param off_dly Optional delay in seconds after which the sensor’s state will be updated back
     * to off
     * @param attribs Optional alternative attribute group (by default all entities reference the
     * "device" attributes group)
     */
    void addMultiBinarySensor(String name, String value, int count, String human = "",
                              String dev_cla = "", String unit = "", String icon = "",
                              String val_tpl = "", int exp_aft = -1, bool frc_upd = false,
                              int off_dly = -1, String attribs = "") {
        if (count > 1) {
            addGenericSensor(BinarySensor, name, value, -count, human, dev_cla, unit, icon, val_tpl,
                             exp_aft, frc_upd, off_dly, attribs);
        }
    }

  protected:
    void onMqttConfig(String topic, String msg, String originator) {
        if (originator == "mqtt") {
            return;
        }
        pathPrefix = shift(msg, '+');
        lastWillTopic = shift(msg, '+');
        lastWillMessage = shift(msg, '+');
    }

    void onMqttState(String topic, String msg, String originator) {
        if (originator == "mqtt") {
            return;
        }
        bool previous = connected;
        connected = msg == "connected";
        if (connected != previous) {
            updateHA();
        }
    }

    void onNetNetwork(String topic, String msg, String originator) {
        if (originator == "mqtt") {
            return;
        }
        JSONVar mqttMsg = JSON.parse(msg);
        if (JSON.typeof(mqttMsg) == "undefined") {
            return;
        }
        if (!strcmp((const char *)mqttMsg["state"], "connected")) {
            ipAddress = (const char *)mqttMsg["ip"];
            macAddress = (const char *)mqttMsg["mac"];
            hostName = (const char *)mqttMsg["hostname"];
        }
    }

    void onNetRssi(String topic, String msg, String originator) {
        if (originator == "mqtt") {
            return;
        }
        rssiVal = parseLong(msg, 0);
        if (autodiscovery) {
            publishAttribs();
        }
    }

    void onCommand(String topic, String msg) {
        if (topic == "get") {
            publishState();
        } else if (topic == "set") {
            msg.trim();
            msg.toLowerCase();
            if (msg == "on" || msg == "true") {
                setAutoDiscovery(true);
            } else if (msg == "off" || msg == "false") {
                setAutoDiscovery(false);
            }
        }
    }

    void addGenericActor(DeviceType type, String name, int channel, String human = "",
                         String dev_cla = "", String icon = "", String attribs = "", String effects = "") {
        size_t len = name.length() + human.length() + dev_cla.length() + icon.length() +
                     attribs.length() + effects.length() + 6;
        Entity entity = {};
        entity.name = (const char *)malloc(len);
        if (entity.name) {
            entity.type = type;
            entity.channel = channel;
            entity.human = copyfieldgetnext(entity.name, name);
            entity.dev_cla = copyfieldgetnext(entity.human, human);
            entity.icon = copyfieldgetnext(entity.dev_cla, dev_cla);
            entity.attribs = copyfieldgetnext(entity.icon, icon);
            entity.effects = copyfieldgetnext(entity.attribs, attribs);
            copyfieldgetnext(entity.effects, effects);
            if (entityConfigs.add(entity) == -1) {
                free((void *)entity.name);
            }
        }
    }

    void addGenericSensor(DeviceType type, String name, String value, int channel,
                          String human = "", String dev_cla = "", String unit = "",
                          String icon = "", String val_tpl = "", int exp_aft = -1,
                          bool frc_upd = false, int off_dly = -1, String attribs = "") {
        size_t len = name.length() + value.length() + human.length() + dev_cla.length() +
                     unit.length() + icon.length() + val_tpl.length() + attribs.length() + 8;

        Entity entity = {};
        entity.name = (const char *)malloc(len);
        if (entity.name) {
            entity.type = type;
            entity.channel = channel;
            entity.value = copyfieldgetnext(entity.name, name);
            entity.human = copyfieldgetnext(entity.value, value);
            entity.dev_cla = copyfieldgetnext(entity.human, human);
            entity.unit = copyfieldgetnext(entity.dev_cla, dev_cla);
            entity.icon = copyfieldgetnext(entity.unit, unit);
            entity.val_tpl = copyfieldgetnext(entity.icon, icon);
            entity.attribs = copyfieldgetnext(entity.val_tpl, val_tpl);
            copyfieldgetnext(entity.attribs, attribs);
            entity.off_dly = off_dly;
            entity.exp_aft = exp_aft;
            entity.frc_upd = frc_upd;
            if (entityConfigs.add(entity) == -1) {
                free((void *)entity.name);
            }
        }
    }

    static const char *getDeviceClass(DeviceType type) {
        switch (type) {
        default:
            return "sensor";
        case DeviceType::BinarySensor:
            return "binary_sensor";
        case DeviceType::Light:
        case DeviceType::LightDim:
        case DeviceType::LightRGB:
        case DeviceType::LightRGBW:
        case DeviceType::LightRGBWW:
        case DeviceType::LightWW:
            return "light";
        case DeviceType::Switch:
            return "switch";
        }
    }

    String getConfigTopic(DeviceType type, const char *uniq_id) {
        return haTopicConfig + getDeviceClass(type) + "/" + uniq_id + "/config";
    }

    void flushDeviceConfig(DeviceType type, JSONVar &msg) {
        msg["dev"]["ids"][0] = deviceId;
        msg["dev"]["name"] = deviceName;
        msg["dev"]["mf"] = deviceManufacturer;
        msg["dev"]["mdl"] = deviceModel;
        msg["dev"]["sw"] = deviceVersion;
        // too much of goodness...
        // msg["dev"]["cns"][0][0] = "IP";
        // msg["dev"]["cns"][0][1] = ipAddress;
        // msg["dev"]["cns"][1][0] = "Host";
        // msg["dev"]["cns"][1][1] = hostName;
        pSched->publish(getConfigTopic(type, msg["uniq_id"]), JSON.stringify(msg));
        // Serial.println("TOPIC:" + getConfigTopic(type, msg["uniq_id"]));
        // Serial.println("      " + JSON.stringify(msg));
    }

    void publishDeviceConfig() {
        JSONVar msg;
        msg["~"] = pathPrefix + "/";
        msg["name"] = hostName + " Status";
        msg["stat_t"] = "~ha/attribs/device";
        msg["avty_t"] = "~mqtt/state";
        msg["pl_avail"] = "connected";
        msg["pl_not_avail"] = lastWillMessage;
        msg["json_attr_t"] = "~ha/attribs/device";
        msg["unit_of_meas"] = "%";
        msg["val_tpl"] = "{{value_json['RSSI']}}";
        msg["ic"] = "mdi:information-outline";
        msg["uniq_id"] = deviceId + "_status";
        flushDeviceConfig(DeviceType::Sensor, msg);
    }

    static String getEntityName(Entity &entity) {
        if (entity.human && *entity.human) {
            return entity.human;
        } else if (entity.value && *entity.value) {
            return String(entity.name) + " " + entity.value;
        } else {
            return entity.name;
        }
    }

    String getEntityKey(Entity &entity) {
        String retVal;
        if (entity.value && *entity.value) {
            retVal = deviceId + "_" + entity.name + "_" + entity.value;
        } else {
            retVal = deviceId + "_" + entity.name;
        }
        retVal.replace(" ", "_");
        return retVal;
    }

    static String getEntityTopic(Entity &entity) {
        String entityTopic = entity.name;
        entityTopic.concat("/");
        entityTopic.concat(getDeviceClass(entity.type));
        entityTopic.replace(" ", "_");
        return entityTopic;
    }

    void publishConfigs() {
        // device config
        publishDeviceConfig();

        // entity configs
        for (unsigned int i = 0; i < entityConfigs.length(); i++) {
            publishConfig(entityConfigs[i]);
        }
    }

    void publishConfig(Entity &entity) {
        String name = getEntityName(entity);
        String key = getEntityKey(entity);
        String topic = getEntityTopic(entity);

        if (entity.channel == -1) {
            publishConfig(entity, name, key, topic);
        } else if (entity.channel < -1) {
            for (int i = 0; i < abs(entity.channel); i++) {
                publishConfig(entity, name + "." + i, key + "_" + i, topic + "/" + i);
            }
        } else {
            int i = entity.channel;
            publishConfig(entity, name + "." + i, key + "_" + i, topic + "/" + i);
        }
    }

    void publishConfig(Entity &entity, String name, String key, String topic) {
        JSONVar msg;
        msg["~"] = pathPrefix + "/";
        msg["name"] = hostName + " " + name;
        msg["uniq_id"] = key;
        msg["avty_t"] = "~mqtt/state";
        msg["pl_avail"] = "connected";
        msg["pl_not_avail"] = lastWillMessage;
        msg["json_attr_t"] = "~" + haTopicAttrib + (*entity.attribs ? entity.attribs : "device");
        if (*entity.dev_cla) {
            msg["dev_cla"] = entity.dev_cla;
        }
        if (*entity.icon) {
            msg["ic"] = entity.icon;
        }
        switch (entity.type) {
        // case DeviceType::Cover:
        //     publishCoverConfig(msg, entity, topic);
        //     break;
        case DeviceType::Light:
        case DeviceType::LightDim:
        case DeviceType::LightRGB:
        case DeviceType::LightRGBW:
        case DeviceType::LightRGBWW:
        case DeviceType::LightWW:
            publishLightConfig(msg, entity, topic);
            break;
        case DeviceType::Sensor:
        case DeviceType::BinarySensor:
            publishSensorConfig(msg, entity, topic);
            break;
        case DeviceType::Switch:
            publishSwitchConfig(msg, entity, topic);
            break;
        default:
            break;
        }
    }

    void publishLightConfig(JSONVar &msg, Entity &entity, String &topic) {
        msg["stat_t"] = "~" + topic + "/state";
        msg["cmd_t"] = hostName + "/" + topic + "/set";
        msg["payload_on"] = "on";
        msg["payload_off"] = "off";

        if (entity.type == LightDim || entity.type == LightWW) {
            // add support for brightness
            msg["bri_cmd_t"] = hostName + "/" + topic + "/set";
            msg["bri_scl"] = "100";
            msg["bri_stat_t"] = "~" + topic + "/unitbrightness";
            msg["bri_val_tpl"] = "{{ value | float * 100 | round(0) }}";
            msg["on_cmd_type"] = "brightness";
        }
        if (entity.type == LightRGB || entity.type == LightRGBW || entity.type == LightRGBWW) {
            // add support for brightness
            msg["bri_cmd_t"] = hostName + "/" + topic + "/set";
            msg["bri_scl"] = "100";
            msg["bri_stat_t"] = "~" + topic + "/unitbrightness";
            msg["bri_val_tpl"] = "{{ value | float * 100 | round(0) }}";
            msg["on_cmd_type"] = "first";
            // color
            msg["color_mode"] = true;
            switch (entity.type) {
            case LightRGB:
                msg["supported_color_modes"] = {"rgb"};
                break;
            case LightRGBW:
                msg["supported_color_modes"] = {"rgbw"};
                break;
            case LightRGBWW:
                msg["supported_color_modes"] = {"rgbww"};
                break;
            default:
                break;  // compiler shutup.
            }
            msg["rgb_cmd_t"] = hostName + "/" + topic + "/color/set";
            msg["rgb_stat_t"] = "~" + topic + "/color";
            if (strcmp(entity.effects, "")) {  // Effects are defined:
                msg["effect_command_topic"] = hostName + "/" + topic + "/effect/set";
                msg["effect_state_topic"] = "~" + topic + "/effect";
                // entity.effects contains a string with a comma-separated list of effect-names (e.g. "effect 1, effect2 "), make them into a JSON array:
                JSONVar elst;
                int ind, n;
                n = 0;
                String effs = entity.effects;
                ind = effs.indexOf(',');
                while (ind != -1) {
                    String tmp = effs.substring(0, ind);
                    tmp.trim();
                    elst[n] = tmp;
                    n += 1;
                    effs = effs.substring(ind + 1);
                    ind = effs.indexOf(',');
                }
                String tmp = effs;
                tmp.trim();
                elst[n] = tmp;
                msg["effect_list"] = elst;  // JSON array, each element contains the name of one effect.
            }
        }
        flushDeviceConfig(entity.type, msg);
    }

    void publishSensorConfig(JSONVar &msg, Entity &entity, String &topic) {
        msg["stat_t"] = "~" + topic + "/" + entity.value;
        if (*entity.val_tpl) {
            msg["val_tpl"] = entity.val_tpl;
        }
        if (*entity.unit) {
            msg["unit_of_meas"] = entity.unit;
        }
        if (entity.exp_aft != -1) {
            msg["exp_aft"] = entity.exp_aft;
        }
        if (entity.frc_upd) {
            msg["frc_upd"] = "true";
        }
        flushDeviceConfig(entity.type, msg);
    }

    void publishSwitchConfig(JSONVar &msg, Entity &entity, String &topic) {
        msg["stat_t"] = "~" + topic + "/state";
        msg["cmd_t"] = hostName + "/" + topic + "/set";
        msg["payload_on"] = "on";
        msg["payload_off"] = "off";
        if (*entity.dev_cla) {
            msg["dev_cla"] = entity.dev_cla;
        }
        flushDeviceConfig(entity.type, msg);
    }

    void unpublishConfigs() {
        // device config
        pSched->publish("!homeassistant/sensor/" + deviceId + "_status/config");

        // entity configs
        for (unsigned int i = 0; i < entityConfigs.length(); i++) {
            unpublishConfig(entityConfigs[i]);
        }
    }

    void unpublishConfig(Entity &entity) {
        String entityKey = getEntityKey(entity);
        String configTopic = haTopicConfig + getDeviceClass(entity.type) + "/";
        if (entity.channel == -1) {
            pSched->publish(configTopic + entityKey + "/config");
        } else if (entity.channel < -1) {
            for (int channel = 0; channel < abs(entity.channel); channel++) {
                pSched->publish(configTopic + entityKey + "_" + channel + "/config");
            }
        } else {
            pSched->publish(configTopic + entityKey + "_" + entity.channel + "/config");
        }
    }

    void publishAttribs() {
        JSONVar msg = JSON.parse("{}");
        msg["RSSI"] = String(WifiGetRssiAsQuality(rssiVal));
        msg["Signal (dBm)"] = String(rssiVal);
        msg["Mac"] = macAddress;
        msg["IP"] = ipAddress;
        msg["Host"] = hostName;
        for (unsigned int i = 0; i < attribGroups.length(); i++) {
            msg["Manufacturer"] = attribGroups[i].manufacturer;
            msg["Model"] = attribGroups[i].model;
            msg["Version"] = attribGroups[i].version;
            pSched->publish(haTopicAttrib + attribGroups[i].name, JSON.stringify(msg));
        }
    }

    void unpublishAttribs() {
        for (unsigned int i = 0; i < attribGroups.length(); i++) {
            pSched->publish(haTopicAttrib + attribGroups[i].name);
        }
    }

    void publishState() {
        pSched->publish("ha/state", autodiscovery ? "on" : "off");
    }

    void updateHA() {
        if (connected) {
            if (autodiscovery) {
                publishAttribs();
                publishConfigs();
            } else {
                unpublishConfigs();
                unpublishAttribs();
            }
        }
    }

    static const char *copyfieldgetnext(const char *field, String &value) {
        strcpy((char *)field, value.c_str());
        return field + value.length() + 1;
    }

    static int WifiGetRssiAsQuality(int rssi) {
        int quality = 0;

        if (rssi <= -100) {
            quality = 0;
        } else if (rssi >= -50) {
            quality = 100;
        } else {
            quality = 2 * (rssi + 100);
        }
        return quality;
    }
};

const char *HomeAssistant::version = "0.1.0";

}  // namespace ustd
