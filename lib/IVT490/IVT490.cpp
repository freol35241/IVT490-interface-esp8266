#include "IVT490.h"
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

    int parse_IVT490(String raw, IVT490State &parsed)
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
        parsed.GT1 = 0.1 * split[1].toFloat();
        parsed.GT2_heatpump = 0.1 * split[2].toFloat();
        parsed.GT3_1 = 0.1 * split[2].toFloat();
        parsed.GT3_2 = 0.1 * split[3].toFloat();
        parsed.GT3_3 = 0.1 * split[4].toFloat();
        parsed.GT5 = 0.1 * split[6].toFloat();
        parsed.GT6 = 0.1 * split[7].toFloat();
        parsed.GT3_4 = 0.1 * split[8].toFloat();
        parsed.GP3 = (bool)split[9].toInt();
        parsed.GP2 = (bool)split[10].toInt();
        parsed.GP1 = (bool)split[11].toInt();
        parsed.vacation = (bool)split[12].toInt();
        parsed.compressor = (bool)split[13].toInt();
        parsed.SV1_open = (bool)split[14].toInt();
        parsed.SV1_close = (bool)split[15].toInt();
        parsed.P1 = (bool)split[16].toInt();
        parsed.fan = (bool)split[17].toInt();
        parsed.alarm = (bool)split[18].toInt();
        parsed.P2 = (bool)split[19].toInt();
        parsed.GT1_LLT = 0.1 * split[20].toFloat();
        parsed.GT1_LL = 0.1 * split[21].toFloat();
        parsed.GT1_target = 0.1 * split[22].toFloat();
        parsed.GT1_UL = 0.1 * split[23].toFloat();
        parsed.GT3_2_LL = 0.1 * split[24].toFloat();
        parsed.GT3_2_ULT = 0.1 * split[25].toFloat();
        parsed.GT3_2_UL = 0.1 * split[26].toFloat();
        parsed.GT3_3_LL = 0.1 * split[27].toFloat();
        parsed.GT3_3_target = 0.1 * split[28].toFloat();
        parsed.electricity_supplement = 0.1 * split[33].toFloat();

        return 0;
    }

    DynamicJsonDocument serialize_IVT490State(const IVT490State &state)
    {
        LOG_INFO("Serializing IVT490State");
        DynamicJsonDocument doc(4096);

        doc["GT1"] = state.GT1;
        doc["GT1_target"] = state.GT1_target;
        doc["GT1_LLT"] = state.GT1_LLT;
        doc["GT1_LL"] = state.GT1_LL;
        doc["GT1_UL"] = state.GT1_UL;
        doc["GT2_heatpump"] = state.GT2_heatpump;
        doc["GT2_sensor"] = state.GT2_sensor;
        doc["GT3_1"] = state.GT3_1;
        doc["GT3_2"] = state.GT3_2;
        doc["GT3_2_LL"] = state.GT3_2_LL;
        doc["GT3_2_ULT"] = state.GT3_2_ULT;
        doc["GT3_3"] = state.GT3_3;
        doc["GT3_3_target"] = state.GT3_3_target;
        doc["GT3_3_LL"] = state.GT3_3_LL;
        doc["GT3_2_UL"] = state.GT3_2_UL;
        doc["GT3_4"] = state.GT3_4;
        doc["GT5"] = state.GT5;
        doc["GT6"] = state.GT6;

        doc["electricity_supplement"] = state.electricity_supplement;

        doc["GP1"] = state.GP1;
        doc["GP2"] = state.GP2;
        doc["GP3"] = state.GP3;
        doc["compressor"] = state.compressor;
        doc["vacation"] = state.vacation;
        doc["P1"] = state.P1;
        doc["alarm"] = state.alarm;
        doc["fan"] = state.fan;
        doc["SV1_open"] = state.SV1_open;
        doc["SV1_close"] = state.SV1_close;

        return doc;
    }

}
