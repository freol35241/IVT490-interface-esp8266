#ifndef SMA_H
#define SMA_H

#include <numeric>

namespace IVT490 {
    namespace SMA
    {
        template <typename type_t, unsigned int N>
        class Filter
        {
        private:
            type_t values[N];
            unsigned int idx = 0;
            bool full = false;

        public:
            void input(type_t value)
            {
                this->values[this->idx++] = value;

                if (this->idx >= N)
                {
                    this->idx = 0;
                    this->full = true;
                }
            }
            type_t output(void)
            {
                auto begin = std::begin(this->values);
                auto end = this->full ? std::end(this->values) : std::end(this->values) - (N - this->idx);

                return std::accumulate(begin, end, 0.0) / (end - begin);
            }

            type_t operator()(type_t value)
            {
                this->input(value);
                return this->output();
            }
        };

    }
}
#endif