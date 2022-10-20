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

        float GT1;               // Framledningstemperatur, grader Celsius
        float GT1_target;        // Framledningstemperatur börvärde, grader Celcius
        float GT1_upper_limit;   // Framledningstemperatur övre gräns, grader Celcius
        float GT1_lower_limit;   // Framledningstemperatur undre gräns, grader Celcius
        float GT2_heatpump;      // Utetemperatur input till vp, grader Celcius
        float GT2_sensor;        // Utetemperatur från sensor, grader Celcius
        float GT3_1;             // Tappvarmvatten, grader Celcius
        float GT3_2;             // Varmvatten, grader Celcius
        float GT3_2_upper_limit; // Varmvatten övre gräns, grader Celcius
        float GT3_2_lower_limit; // Varmvatten under gräns, grader Celcius
        float GT3_3;             // Värmevatten, grader Celcius
        float GT3_3_target;      // Värmevatten börvärde, grader Celcius
        float GT3_3_upper_limit; // Värmevatten övre gräns, grader Celcius
        float GT3_3_lower_limit; // Värmevatten undre gräns, grader Celcius
        float GT3_4;             // Extra acc. tank, grader Celcius
        float GT5;               // Innetemperatur, grader Celcius
        float GT6;               // Hetgastemperatur, grader Celcius

        float electricity_supplement; // Eltillskott (elpatron) användning, procent (%) utnyttjande

        bool GP1; // Lågtrycksvakt
        bool GP2; // Högtrycksvakt
        bool compressor;
        bool vacation;
        bool circulation_pump;
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

    template <uint8_t RESOLUTION, unsigned int MAX_RESISTANCE, unsigned int WIPER_RESISTANCE = 125>
    class IVT490ThermistorEmulator
    {
    public:
        IVT490ThermistorEmulator(const float *feedback, uint8_t CS_pin)
        {
            this->feedback = feedback;
            this->pot.begin(CS_pin);
        }

        void update_target_ptr(float *target)
        {
            static unsigned int STEPS = pow(2, RESOLUTION);

            this->target = target;

            LOG_DEBUG("Setting new target ptr, with current value:", *target);

            // Write an initial guess based on reverse interpolation of the NTC values
            auto wanted_resistance = NTC_interpolate_resistance(*this->target);

            // This expects the connections to be made over PB0-PW0
            this->wiper_value = (uint8_t)(STEPS * (wanted_resistance - WIPER_RESISTANCE) / MAX_RESISTANCE);

            LOG_DEBUG("Setting new wiper value:", this->wiper_value);
            this->pot.setWiper(this->wiper_value);
        }

        void adjust()
        {
            // This expects the connection to be made over PB0-PW0
            int correction = (int)(5 * (*this->target - *this->feedback));
            this->wiper_value = (uint8_t)max(min(this->wiper_value - correction, 255), 0);

            LOG_DEBUG("Setting new wiper value:", this->wiper_value);
            this->pot.setWiper(this->wiper_value);
        }

    private:
        float *target;
        const float *feedback;
        MCP41_Simple pot;
        uint8_t wiper_value;
    };

}
#endif