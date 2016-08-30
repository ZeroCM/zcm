namespace zcm {

#include <cassert>

class Filter
{
  private:
    double obs = 0;
    double x1 = 0;
    double x2 = 0;
    double natFreq, damping;

  public:
    Filter(double naturalFreq, double dampingFactor) :
        natFreq(naturalFreq), damping(dampingFactor) {}

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
        // RRR: could potentially save some calculation by pre-calculating natFreq^2 and
        //      2*damping*natFreq, but not a huge issue if you don't want to
        double dx2 = -(natFreq * natFreq) * x1 - 2 * damping * natFreq * x2 + obs;
        x1 += dt * dx1;
        x2 += dt * dx2;
    }

    inline double lowPass() const
    {
        return natFreq * natFreq * x1;
    }

    inline double bandPass() const
    {
        return 2 * damping * natFreq * x2;
    }

    inline double highPass() const
    {
        return obs - (lowPass() + bandPass());
    }

    inline void operator()(double obs, double dt) { newObs(obs, dt); }

    enum FilterMode { LOW_PASS, BAND_PASS, HIGH_PASS };
    inline double operator[](enum FilterMode m) const
    {
        // RRR: stylistically, I think it's fine to drop the break statements from cases that
        //      always return
        switch (m) {
            case FilterMode::LOW_PASS:
                return lowPass();
                break;
            case FilterMode::BAND_PASS:
                return bandPass();
                break;
            case FilterMode::HIGH_PASS:
                return highPass();
                break;
        }
        // RRR: you actually can't get here (known at compile time) because you are doing
        //      a switch on an enum, so you could delete this
        assert(false && "Should not be able to get here");
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
