/******************************************************************************
 *
 * Header file for doing timing
 *
 *****************************************************************************/

#ifndef KRIPKE_TIMING_H__
#define KRIPKE_TIMING_H__

#include<Kripke.h>

#include <string>
#include <vector>
#include <map>


struct Timer {
  Timer() :
    started(false),
    start_time(0.0),
    total_time(0.0),
    count(0)
  {}
  
  bool started;
  double start_time;
  double total_time;
  size_t count;

#if KRIPKE_USE_PAPI
  std::vector<size_t> papi_total;
#endif
};

class Timing {
  public:
    void start(std::string const &name);
    void stop(std::string const &name);
    
    void stopAll(void);
    void clear(void);
    
    void print(void) const;
    
    double getTotal(std::string const &name) const;

    void setPapiEvents(std::vector<std::string> names);

  private:
    typedef std::map<std::string, Timer> TimerMap;
    TimerMap timers;
#if KRIPKE_USE_PAPI
    std::vector<std::string> papi_names;
    std::vector<int> papi_event;
#endif
};


#include<stdio.h>

// Aides timing a block of code, with automatic timer stopping
class BlockTimer {
  public:
  inline BlockTimer(Timing &timer_obj, std::string const &timer_name) :
      timer(timer_obj),
      name(timer_name)
  {
    //printf("Starting Timer %s\n", name.c_str());
      timer.start(name);
  }
  inline ~BlockTimer(){
    //printf("Stopping Timer %s\n", name.c_str());
    timer.stop(name);
  }

  private:
      Timing &timer;
      std::string name;
};

#define BLOCK_TIMER(TIMER, NAME) BlockTimer BLK_TIMER_##NAME(TIMER, #NAME);


#endif
