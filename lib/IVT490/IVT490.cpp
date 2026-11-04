#include "IVT490.h"
#include "SMA.h"
#include "MultiMap.h"

#define IVT490_NO_OF_ITEMS_IN_SENTENCE 37

namespace IVT490
{
    static const int NTC_number_of_values = 27;
    // Ohm
    float NTC_resistances[NTC_number_of_values] = {-154300,
                                                   -111700,
                                                   -81700,
                                                   -60500,
                                                   -45100,
                                                   -33950,
                                                   -25800,
                                                   -19770,
                                                   -15280,
                                                   -11900,
                                                   -9330,
                                                   -7370,
                                                   -5870,
                                                   -4700,
                                                   -3490,
                                                   -3070,
                                                   -2510,
                                                   -2055,
                                                   -1696,
                                                   -1405,
                                                   -1170,
                                                   -980,
                                                   -824,
                                                   -696,
                                                   -590,
                                                   -503,
                                                   -430};
    // Degrees Celcuis
    float NTC_temperatures[NTC_number_of_values] = {-40,
                                                    -35,
                                                    -30,
                                                    -25,
                                                    -20,
                                                    -15,
                                                    -10,
                                                    -5,
                                                    0,
                                                    5,
                                                    10,
                                                    15,
                                                    20,
                                                    25,
                                                    30,
                                                    35,
                                                    40,
                                                    45,
                                                    50,
                                                    55,
                                                    60,
                                                    65,
                                                    70,
                                                    75,
                                                    80,
                                                    85,
                                                    90

    };

    float NTC_interpolate_temperature(float resistance)
    {
        return multiMap<float>(-resistance, NTC_resistances, NTC_temperatures, NTC_number_of_values);
    }

    float NTC_interpolate_resistance(float temperature)
    {
        return -multiMap<float>(temperature, NTC_temperatures, NTC_resistances, NTC_number_of_values);
    }

    // Fields are read in place, by pointer, so a sentence costs no heap.
    // atof/atol stop at the ';' that ends a field, which makes them give
    // exactly what String::toFloat/toInt (atof/atol) gave on the copied
    // substring. Accepted input is unchanged too: at least 36 separators.
    int parse_serial(const char *raw, State::Serial &serial)
    {
        const char *field[IVT490_NO_OF_ITEMS_IN_SENTENCE];
        const char *p = raw;

        for (int item = 0; item < IVT490_NO_OF_ITEMS_IN_SENTENCE; item++)
        {
            field[item] = p;
            if (item == IVT490_NO_OF_ITEMS_IN_SENTENCE - 1)
            {
                break;
            }

            const char *sep = strchr(p, ';');
            if (sep == nullptr)
            {
                LOG_ERROR("Received raw string did not have correct length (37)!");
                return -1;
            }
            p = sep + 1;
        }

        auto f = [&field](int i) { return (float)atof(field[i]); };
        auto b = [&field](int i) { return (bool)atol(field[i]); };

        // Parsing and interpreting each field
        serial.GT1 = 0.1 * f(1);
        serial.GT2 = 0.1 * f(2);
        serial.GT3_1 = 0.1 * f(3);
        serial.GT3_2 = 0.1 * f(4);
        serial.GT3_3 = 0.1 * f(5);
        serial.GT5 = 0.1 * f(6);
        serial.GT6 = 0.1 * f(7);
        serial.GT3_4 = 0.1 * f(8);
        serial.GP3 = b(9);
        serial.GP2 = b(10);
        serial.GP1 = b(11);
        serial.vacation = b(12);
        serial.compressor = b(13);
        serial.SV1_open = b(14);
        serial.SV1_close = b(15);
        serial.P1 = b(16);
        serial.fan = b(17);
        serial.alarm = b(18);
        serial.P2 = b(19);
        serial.GT1_LLT = 0.1 * f(20);
        serial.GT1_LL = 0.1 * f(21);
        serial.GT1_target = 0.1 * f(22);
        serial.GT1_UL = 0.1 * f(23);
        serial.GT3_2_LL = 0.1 * f(24);
        serial.GT3_2_ULT = 0.1 * f(25);
        serial.GT3_2_UL = 0.1 * f(26);
        serial.GT3_3_LL = 0.1 * f(27);
        serial.GT3_3_target = 0.1 * f(28);
        serial.electricity_supplement = 0.1 * f(33);

        return 0;
    }

    void State::serialize(JsonDocument &doc) const
    {
        LOG_INFO("Serializing IVT490::State");
        doc.clear();

        auto serial = this->serial;

        doc["serial"]["GT1"] = serial.GT1;
        doc["serial"]["GT1_target"] = serial.GT1_target;
        doc["serial"]["GT1_LLT"] = serial.GT1_LLT;
        doc["serial"]["GT1_LL"] = serial.GT1_LL;
        doc["serial"]["GT1_UL"] = serial.GT1_UL;
        doc["serial"]["GT2"] = serial.GT2;
        doc["serial"]["GT3_1"] = serial.GT3_1;
        doc["serial"]["GT3_2"] = serial.GT3_2;
        doc["serial"]["GT3_2_LL"] = serial.GT3_2_LL;
        doc["serial"]["GT3_2_UL"] = serial.GT3_2_UL;
        doc["serial"]["GT3_2_ULT"] = serial.GT3_2_ULT;
        doc["serial"]["GT3_3"] = serial.GT3_3;
        doc["serial"]["GT3_3_target"] = serial.GT3_3_target;
        doc["serial"]["GT3_3_LL"] = serial.GT3_3_LL;
        doc["serial"]["GT3_4"] = serial.GT3_4;
        doc["serial"]["GT5"] = serial.GT5;
        doc["serial"]["GT6"] = serial.GT6;

        doc["serial"]["electricity_supplement"] = serial.electricity_supplement;

        doc["serial"]["GP1"] = serial.GP1;
        doc["serial"]["GP2"] = serial.GP2;
        doc["serial"]["GP3"] = serial.GP3;
        doc["serial"]["compressor"] = serial.compressor;
        doc["serial"]["vacation"] = serial.vacation;
        doc["serial"]["P1"] = serial.P1;
        doc["serial"]["alarm"] = serial.alarm;
        doc["serial"]["fan"] = serial.fan;
        doc["serial"]["SV1_open"] = serial.SV1_open;
        doc["serial"]["SV1_close"] = serial.SV1_close;

        doc["GT2"]["raw"] = this->GT2.raw;
        doc["GT2"]["filtered"] = this->GT2.filtered;

        doc["GT3_2"]["raw"] = this->GT3_2.raw;
        doc["GT3_2"]["filtered"] = this->GT3_2.filtered;
    }

}
