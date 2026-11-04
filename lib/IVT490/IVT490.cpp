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

    int parse_serial(String raw, State::Serial &serial)
    {
        String split[IVT490_NO_OF_ITEMS_IN_SENTENCE];

        // Splitting raw string
        auto remainder = raw;

        for (int item = 0; item < IVT490_NO_OF_ITEMS_IN_SENTENCE; item++)
        {
            auto ix = remainder.indexOf(';');

            if (ix == -1)
            {
                if (item == IVT490_NO_OF_ITEMS_IN_SENTENCE - 1)
                {
                    split[item] = remainder;
                    break;
                }
                else
                {
                    LOG_ERROR("Received raw string did not have correct length (37)!");
                    return -1;
                }
            }

            split[item] = remainder.substring(0, ix);

            remainder = remainder.substring(ix + 1);
        }

        // Parsing and interpreting each substring
        serial.GT1 = 0.1 * split[1].toFloat();
        serial.GT2 = 0.1 * split[2].toFloat();
        serial.GT3_1 = 0.1 * split[3].toFloat();
        serial.GT3_2 = 0.1 * split[4].toFloat();
        serial.GT3_3 = 0.1 * split[5].toFloat();
        serial.GT5 = 0.1 * split[6].toFloat();
        serial.GT6 = 0.1 * split[7].toFloat();
        serial.GT3_4 = 0.1 * split[8].toFloat();
        serial.GP3 = (bool)split[9].toInt();
        serial.GP2 = (bool)split[10].toInt();
        serial.GP1 = (bool)split[11].toInt();
        serial.vacation = (bool)split[12].toInt();
        serial.compressor = (bool)split[13].toInt();
        serial.SV1_open = (bool)split[14].toInt();
        serial.SV1_close = (bool)split[15].toInt();
        serial.P1 = (bool)split[16].toInt();
        serial.fan = (bool)split[17].toInt();
        serial.alarm = (bool)split[18].toInt();
        serial.P2 = (bool)split[19].toInt();
        serial.GT1_LLT = 0.1 * split[20].toFloat();
        serial.GT1_LL = 0.1 * split[21].toFloat();
        serial.GT1_target = 0.1 * split[22].toFloat();
        serial.GT1_UL = 0.1 * split[23].toFloat();
        serial.GT3_2_LL = 0.1 * split[24].toFloat();
        serial.GT3_2_ULT = 0.1 * split[25].toFloat();
        serial.GT3_2_UL = 0.1 * split[26].toFloat();
        serial.GT3_3_LL = 0.1 * split[27].toFloat();
        serial.GT3_3_target = 0.1 * split[28].toFloat();
        serial.electricity_supplement = 0.1 * split[33].toFloat();

        return 0;
    }

    DynamicJsonDocument State::serialize() const
    {
        LOG_INFO("Serializing IVT490::State");
        DynamicJsonDocument doc(4096);

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

        return doc;
    }

}
