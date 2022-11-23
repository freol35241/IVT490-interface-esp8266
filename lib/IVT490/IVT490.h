#ifndef IVT490_H
#define IVT490_H

#include <Arduino.h>
#include <MCP41_Simple.h>
#include <MCP_ADC.h>
#include <ArduinoJson.h>
#include <DebugLog.h>

namespace IVT490
{

    struct IVT490State
    {

        float GT1;          // Framledningstemperatur, grader Celsius
        float GT1_target;   // Framledningstemperatur börvärde, grader Celcius
        float GT1_UL;       // Framledningstemperatur övre gräns, grader Celcius
        float GT1_LL;       // Framledningstemperatur undre gräns, grader Celcius
        float GT1_LLT;      // (??) Framledningstemperatur undre gräns, grader Celcius
        float GT2_heatpump; // Utetemperatur input till vp, grader Celcius
        float GT2_sensor;   // Utetemperatur från sensor, grader Celcius
        float GT3_1;        // Tappvarmvatten, grader Celcius
        float GT3_2;        // Varmvatten, grader Celcius
        float GT3_2_ULT;    // Varmvatten övre gräns, grader Celcius
        float GT3_2_LL;     // Varmvatten under gräns, grader Celcius
        float GT3_3;        // Värmevatten, grader Celcius
        float GT3_3_target; // Värmevatten börvärde, grader Celcius
        float GT3_2_UL;     // Värmevatten övre gräns, grader Celcius
        float GT3_3_LL;     // Värmevatten undre gräns, grader Celcius
        float GT3_4;        // Extra acc. tank, grader Celcius
        float GT5;          // Innetemperatur, grader Celcius
        float GT6;          // Hetgastemperatur, grader Celcius

        float electricity_supplement; // Eltillskott (elpatron) användning, procent (%) utnyttjande

        bool GP1; // Lågtrycksvakt
        bool GP2; // Högtrycksvakt
        bool GP3; // Avfrostningsvakt
        bool compressor;
        bool vacation;
        bool P1; // Circulation pump
        bool P2; // External pump
        bool alarm;
        bool fan;
        bool SV1_open;
        bool SV1_close;
    };

    float NTC_interpolate_temperature(float resistance);
    float NTC_interpolate_resistance(float temperature);

    int parse_IVT490(String raw, IVT490State &parsed);

    DynamicJsonDocument serialize_IVT490State(const IVT490State &state);

    inline float heating_curve(float slope, float outdoor_temperature)
    {
        return 20 + (-0.16 * slope) * (outdoor_temperature - 20);
    }

    inline float inverse_heating_curve(float slope, float feed_temperature)
    {
        return (feed_temperature - 20) / (-0.16 * slope) + 20;
    }

    // The IVT490ThermistorReader expects the following circuit
    //   Vs
    //   |
    //  NTC
    //   |
    //   o ----- Vadc
    //   |
    //  R_0
    //   |
    //  GND

    template <unsigned int R_0>
    class IVT490ThermistorReader
    {
    public:
        IVT490ThermistorReader(uint8_t CS_pin, uint8_t channel)
        {
            this->adc.begin(CS_pin);
            this->channel = channel;
        }
        float read()
        {
            float adc_value = this->adc.analogRead(this->channel);
            float max_value = this->adc.maxValue();

            LOG_DEBUG("ADC value (channel", this->channel, "):", adc_value);
            LOG_DEBUG("ADC max value:", max_value);

            float resistance = adc_value > 0.0f ? R_0 * ((max_value / adc_value) - 1) : std::numeric_limits<float>::max();

            LOG_DEBUG("Resistance:", resistance, "ohm");

            return NTC_interpolate_temperature(resistance);
        }

    private:
        MCP3208 adc;
        uint8_t channel;
    };

    // The IVT490ThermistorEmulator expects the following circuit
    //
    //            MCP41XXX
    //
    //          -------
    //      CS -|     |- VDD
    //     SCK -|     |- PB0 ---- Heatpump
    //      SI -|     |- PW0 ---- connection
    //     VSS -|     |- PA0
    //          -------

    template <unsigned int VALIDITY, uint8_t RESOLUTION, unsigned int MAX_RESISTANCE, unsigned int WIPER_RESISTANCE = 125>
    class IVT490ThermistorEmulator
    {
    public:
        IVT490ThermistorEmulator(uint8_t CS_pin)
        {
            this->target_birth_time = 0;

            this->pot.begin(CS_pin);
            this->set_wiper_value_from_temperature(6);
        }

        void set_wiper_value_from_temperature(float temperature)
        {
            static unsigned int STEPS = pow(2, RESOLUTION);
            LOG_DEBUG("Calculating wiper value for temperature:", temperature);

            // Write an initial guess based on reverse interpolation of the NTC values
            auto wanted_resistance = NTC_interpolate_resistance(temperature);

            LOG_DEBUG("    equalling wanted resistance:", wanted_resistance);

            wanted_resistance += this->resistance_offset;
            LOG_DEBUG("    adding resistance offset:", this->resistance_offset);
            LOG_DEBUG("    resulting in wanted resistance:", wanted_resistance);

            // This expects the connections to be made over PB0-PW0
            float fraction = (wanted_resistance - WIPER_RESISTANCE) / MAX_RESISTANCE;
            fraction = max(0.0f, min(1.0f, fraction)); // Capping to usable range of digipot
            LOG_DEBUG("    equalling capped fraction:", fraction);

            uint8_t wiper_value = (uint8_t)((STEPS - 1) * fraction);
            LOG_DEBUG("    equalling wiper value:", wiper_value);

            LOG_INFO("Writing new wiper value to pot:", wiper_value);

            this->pot.setWiper(wiper_value);
        }

        void set_external_target_value(float target)
        {
            LOG_INFO("Setting new external target value:", target);
            this->target = target;
            this->target_birth_time = millis();
            this->set_wiper_value_from_temperature(this->target);
        }

        void set_internal_target_value(float target)
        {
            if (millis() - this->target_birth_time <= VALIDITY && this->target_birth_time != 0)
            {
                LOG_INFO("External target value still valid, disregarding internal target value!");
                return;
            }

            LOG_INFO("Setting new internal target value:", target);
            this->target = target;
            this->set_wiper_value_from_temperature(this->target);
        }

        void adjust_correction(float feedback)
        {
            LOG_INFO("Adjusting thermistor emulator correction based on feedback.");
            LOG_DEBUG("    Target value:", this->target);
            LOG_DEBUG("    Feedback value:", feedback);

            // This expects the connection to be made over PB0-PW0
            auto resistance_at_target = NTC_interpolate_resistance(this->target);
            auto resistance_at_feedback = NTC_interpolate_resistance(feedback);
            LOG_DEBUG("    resistance at target:", resistance_at_target);
            LOG_DEBUG("    resistance at feedback:", resistance_at_feedback);

            this->resistance_offset += resistance_at_target - resistance_at_feedback;
            LOG_DEBUG("Current resistance offset:", this->resistance_offset);
        }

    private:
        float target;
        unsigned long target_birth_time;
        MCP41_Simple pot;
        float resistance_offset = 0;
    };

}
#endif