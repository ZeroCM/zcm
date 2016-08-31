namespace zcm {

#include <cassert>

class Filter
{
  private:
    double obs = 0;
    double x1 = 0;
    double x2 = 0;
    double natFreq, damping;
    double natFreq2, natFreqDamping_times2;

  public:
    Filter(double naturalFreq, double dampingFactor) :
        natFreq(naturalFreq), damping(dampingFactor)
    {
        natFreq2 = natFreq * natFreq;
        natFreqDamping_times2 = 2 * natFreq * damping;
    }

    static double convergenceTimeToNatFreq(double riseTime, double damping)
    {
        assert(damping > 0.7 &&
               "Cannot estimate natural frequency based on rise "
               "time for underdamped systems");

        /*
         * See below for explanation:
         * https://en.wikipedia.org/wiki/Rise_time#Rise_time_of_damped_second_order_systems
         */
        return (2.230 * damping * damping - 0.078 * damping + 1.12) / riseTime;
    }

    inline void newObs(double obs, double dt)
    {
        this->obs = obs;
        double dx1 = x2;
        double dx2 = -natFreq2 * x1 - natFreqDamping_times2 * x2 + obs;
        x1 += dt * dx1;
        x2 += dt * dx2;
    }

    inline double lowPass() const
    {
        return natFreq2 * x1;
    }

    inline double bandPass() const
    {
        return natFreqDamping_times2 * x2;
    }

    inline double highPass() const
    {
        return obs - (lowPass() + bandPass());
    }

    inline void operator()(double obs, double dt) { newObs(obs, dt); }

    enum FilterMode { LOW_PASS, BAND_PASS, HIGH_PASS };
    inline double operator[](enum FilterMode m) const
    {
        switch (m) {
            case FilterMode::LOW_PASS:
                return lowPass();
            case FilterMode::BAND_PASS:
                return bandPass();
            case FilterMode::HIGH_PASS:
                return highPass();
        }
        return 0;
    }

    friend std::ostream& operator<<(std::ostream& o, const Filter& f)
    {
        o << "Filter: " << std::endl
          << "   Low Pass: " << f.lowPass()  << std::endl
          << "  Band Pass: " << f.bandPass() << std::endl
          << "  High Pass: " << f.highPass() << std::endl;
        return o;
    }
};

}
