#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "IVT490.h"

enum OperatingMode{
    BAU = 1,
    BLOCK = 2,
    BOOST = 3,
};

struct ControlValues {
    float GT2;
    bool enable_vacation_mode;
    float GT3_2;
};


class Controller
{
public:
    Controller(unsigned int validity, float heating_curve_slope):
        validity(validity),
        heating_curve_slope(heating_curve_slope)
        {};

    void set_outdoor_temperature_offset(float offset)
    {
        this->outdoor_temperature_offset = offset;
        this->outdoor_temperature_offset_last_updated = millis();
    }

    bool outdoor_temperature_offset_is_valid()
    {
        return (millis() - this->outdoor_temperature_offset_last_updated <= this->validity && this->outdoor_temperature_offset_last_updated != 0 && !isnan(this->outdoor_temperature_offset));
    }

    void set_summer_temperature_limit(float temperature)
    {
        this->summer_temperature_limit = temperature;
    }

    void set_indoor_temperature(float temperature)
    {
        this->indoor_temperature = temperature;
        this->indoor_temperature_last_updated = millis();
    }

    void set_indoor_temperature_target(float target)
    {
        this->indoor_temperature_target = target;
    }

    void set_indoor_temperature_weight(float weight)
    {
        this->indoor_temperature_weight = weight;
    }

    bool indoor_temperature_is_valid()
    {
        return (
            millis() - this->indoor_temperature_last_updated <= this->validity && this->indoor_temperature_last_updated != 0 && !isnan(this->indoor_temperature));
    }

    void set_feed_temperature_target(float temperature)
    {
        this->feed_temperature_target = temperature;
        this->feed_temperature_target_last_updated = millis();
    }

    bool feed_temperature_target_is_valid()
    {
        return (millis() - this->feed_temperature_target_last_updated <= this->validity && this->feed_temperature_target_last_updated != 0 && !isnan(this->feed_temperature_target));
    }

    void set_operating_mode(OperatingMode mode){
        this->operating_mode = mode;
        this->operating_mode_last_updated = millis();
    }

    std::pair<float, bool> vacation_mode_logic(float outdoor_temperature, float control_value)
    {
        if (this->summer_temperature_limit > 0 && outdoor_temperature < 1.0 && control_value >= this->summer_temperature_limit - 1)
        {
            // We want to have a lower feed temperature than what can be achieved without hitting the summer mode (i.e P1 stops),
            // at the same time as the outdoor temperature is approaching freezing... Not good!
            // The best we can do is to:
            // * Ensure the faked outdoor temperature (i.e. control value) is lower than the summer temperature limit
            // * Enable vacation mode to lower the feed temperature without risking that P1 stops
            LOG_WARN("Controller: Control value adjusted and vacation mode enabled to avoid P1 stopping during freezing temperatures!");
            return std::make_pair(this->summer_temperature_limit - 1.0, true);
        }
        else if (control_value > 21.0)
        {
            // We want to have a lower feed temperature than what the heatpump allows. Not sure if it actually helps to enable
            // vacation mode here but it probably doesnt hurt.
            return std::make_pair(control_value, true);
        }

        // Else, disable vacation mode and return the control_value untouched
        return std::make_pair(control_value, false);
    }

    ControlValues get_control_values(IVT490::State &state)
    {
        ControlValues control_values;
        control_values.enable_vacation_mode = false;

        // GT3_2
        // if (operating_mode == OperatingMode::BAU)
        control_values.GT3_2 = state.GT3_2.filtered;
        
        if (operating_mode == OperatingMode::BLOCK){
            control_values.GT3_2 = state.serial.GT3_2_UL + 1.0;
        }
        else if (operating_mode == OperatingMode::BOOST){
            // We only allow BOOSTING if the boiler temperature is lower than the upper limit
            if (state.GT3_2.filtered < state.serial.GT3_2_UL){
                control_values.GT3_2 = state.serial.GT3_2_LL - 1.0;
            }
        }


        // GT2 and vacation mode
        if (feed_temperature_target_is_valid())
        {
            LOG_INFO("Controller: Feed temperature target is valid!");
            LOG_INFO("Controller: Requested feed temperature:", this->feed_temperature_target);
            control_values.GT2 = IVT490::inverse_heating_curve(
                this->heating_curve_slope,
                this->feed_temperature_target);

            std::tie(control_values.GT2, control_values.enable_vacation_mode) = this->vacation_mode_logic(state.GT2.filtered, control_values.GT2);

            LOG_INFO("Controller: GT2=", control_values.GT2, "vacation=", control_values.enable_vacation_mode, "GT3_2=", control_values.GT3_2);
            return control_values;
        }

        control_values.GT2 = state.GT2.filtered;

        LOG_INFO("Controller: Base outdoor temperature:", control_values.GT2);

        if (outdoor_temperature_offset_is_valid())
        {
            LOG_INFO("Controller: Outdoor temperature offset is valid!");
            LOG_INFO("Controller: Outdoor temperature offset applied:", this->outdoor_temperature_offset);

            control_values.GT2 += this->outdoor_temperature_offset;
        }

        if (indoor_temperature_is_valid())
        {
            LOG_INFO("Controller: Indoor temperature is valid!");
            LOG_INFO("Controller: Requested indoor temperature:", this->indoor_temperature_target);
            LOG_INFO("Controller: Current indoor temperature:", this->indoor_temperature);
            auto indoor_temperature_correction = this->indoor_temperature_weight * (this->indoor_temperature - this->indoor_temperature_target);
            LOG_INFO("Controller: Correction applied:", indoor_temperature_correction);

            control_values.GT2 += indoor_temperature_correction;
        }

        std::tie(control_values.GT2, control_values.enable_vacation_mode) = this->vacation_mode_logic(state.GT2.filtered, control_values.GT2);

        LOG_INFO("Controller: GT2=", control_values.GT2, "vacation=", control_values.enable_vacation_mode, "GT3_2=", control_values.GT3_2);
        return control_values;
    }


    DynamicJsonDocument serialize()
    {
        DynamicJsonDocument doc(1024);

        doc["feed_temperature_target"]["value"] = this->feed_temperature_target;
        doc["feed_temperature_target"]["valid"] = this->feed_temperature_target_is_valid();

        doc["indoor_temperature_feedback"]["value"] = this->indoor_temperature;
        doc["indoor_temperature_feedback"]["valid"] = this->indoor_temperature_is_valid();

        doc["outdoor_temperature_offset"]["value"] = this->outdoor_temperature_offset;
        doc["outdoor_temperature_offset"]["valid"] = this->outdoor_temperature_offset_is_valid();

        doc["indoor_temperature_target"]["value"] = this->indoor_temperature_target;

        doc["operating_mode"] = this->operating_mode;

        return doc;
    }

private:
    unsigned int validity;
    float heating_curve_slope;

    float outdoor_temperature_offset = 0.0;
    unsigned long outdoor_temperature_offset_last_updated = 0;

    float indoor_temperature_target = 20.0;
    float indoor_temperature_weight = 1.0;
    float indoor_temperature;
    unsigned long indoor_temperature_last_updated = 0;

    float feed_temperature_target;
    unsigned long feed_temperature_target_last_updated = 0;

    OperatingMode operating_mode = OperatingMode::BAU;
    unsigned long operating_mode_last_updated = 0;

    float summer_temperature_limit = -1;

};

# endif